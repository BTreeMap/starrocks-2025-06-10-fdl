// Copyright 2021-present StarRocks, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "exprs/case_expr.h"

#include <cstdint>

#include "column/chunk.h"
#include "column/column_builder.h"
#include "column/column_helper.h"
#include "column/column_viewer.h"
#include "column/type_traits.h"
#include "column/vectorized_fwd.h"
#include "common/object_pool.h"
#include "gutil/casts.h"
#include "runtime/runtime_state.h"
#include "simd/mulselector.h"
#include "types/logical_type_infra.h"
#include "util/percentile_value.h"

#ifdef STARROCKS_JIT_ENABLE
#include "exprs/jit/ir_helper.h"
#endif

namespace starrocks {

/**
 * Support Case expression, like:
 *  CASE sex
 *      WHEN '1' THEN 'man'
 *      WHEN '2' THEN 'woman'
 *  ELSE 'other' END
 *
 *  or
 *
 *  CASE WHEN sex = '1' THEN 'man'
 *       WHEN sex = '2' THEN 'woman'
 *  ELSE 'other' END
 *
 *  ELSE is not necessary.
 *  TODO: rewrite the first format to the second due to some advantages: reduce code footprint, compute case-expr
 *  equivalence in vectorization, benefit SIMD-optimization from no-case evaluation.
 */

template <LogicalType WhenType, LogicalType ResultType>
class VectorizedCaseExpr final : public Expr {
public:
    explicit VectorizedCaseExpr(const TExprNode& node)
            : Expr(node), _has_case_expr(node.case_expr.has_case_expr), _has_else_expr(node.case_expr.has_else_expr) {}

    ~VectorizedCaseExpr() override = default;

    Expr* clone(ObjectPool* pool) const override { return pool->add(new VectorizedCaseExpr(*this)); }

    Status open(RuntimeState* state, ExprContext* context, FunctionContext::FunctionStateScope scope) override {
        RETURN_IF_ERROR(Expr::open(state, context, scope));

        // children size check
        if ((_has_case_expr ^ _has_else_expr) == 0) {
            return _children.size() % 2 == 0 ? Status::OK() : Status::InvalidArgument("case when children is error!");
        }

        return _children.size() % 2 == 1 ? Status::OK() : Status::InvalidArgument("case when children is error!");
    }

#ifdef STARROCKS_JIT_ENABLE
    bool is_compilable(RuntimeState* state) const override {
        if (_has_case_expr) {
            return state->can_jit_expr(CompilableExprType::CASE) && IRHelper::support_jit(WhenType) &&
                   IRHelper::support_jit(ResultType);
        } else {
            return state->can_jit_expr(CompilableExprType::CASE) && IRHelper::support_jit(ResultType);
        }
    }

    // It considers the proportion of valuable children, rather than the total score, so it can't depend on an
    // expensive branch. The magic values are resulted from experiments.
    JitScore compute_jit_score(RuntimeState* state) const override {
        JitScore jit_score = {0, 0};
        if (!is_compilable(state)) {
            return jit_score;
        }
        int when_valid = 0;
        int then_valid = 0;
        for (auto i = 0; i < _children.size(); i++) {
            auto tmp = _children[i]->compute_jit_score(state);
            double valid = tmp.score > tmp.num * 0.3;
            if (i == 0) {
                when_valid += valid;
            } else if (i + 1 == _children.size() && _has_else_expr) {
                then_valid += valid;
            } else {
                if ((i + _has_case_expr) % 2 == 0) {
                    when_valid += valid;
                } else {
                    then_valid += valid;
                }
            }
            VLOG_QUERY << i << "-th score: " << tmp.score << " / " << tmp.num << " = "
                       << tmp.score / (tmp.num ? tmp.num : 1) << " "
                       << " valid = " << valid << "  " << _children[i]->jit_func_name(state);
        }
        int expr_num = _children.size() / 2;
        VLOG_QUERY << "JIT score case: when_score =  " << when_valid << " / " << expr_num << " = "
                   << when_valid * 1.0 / expr_num << ", then_score = " << then_valid << " / " << expr_num << " = "
                   << then_valid * 1.0 / expr_num;
        if (when_valid > expr_num * IRHelper::jit_score_ratio || then_valid > expr_num * IRHelper::jit_score_ratio) {
            return {expr_num, expr_num};
        }
        return {0, expr_num};
    }

    StatusOr<LLVMDatum> generate_ir_impl(ExprContext* context, JITContext* jit_ctx) override {
        if constexpr (lt_is_decimal<WhenType> || lt_is_decimal<ResultType>) {
            // TODO(yueyang): Implement case...when in LLVM IR.
            return Status::NotSupported("JIT of case..when..else...end not support");
        } else if constexpr (lt_is_number<WhenType> && lt_is_number<ResultType>) {
            auto& b = jit_ctx->builder;
            auto* head = b.GetInsertBlock();
            auto* join = llvm::BasicBlock::Create(head->getContext(), "join_block", head->getParent());
            LLVMDatum result(b);
            llvm::Value* res = b.CreateAlloca(IRHelper::logical_to_ir_type(b, ResultType).value(), nullptr, "retVal");
            llvm::Value* res_null = b.CreateAlloca(b.getInt8Ty(), nullptr, "retValNull");

            auto* else_block = llvm::BasicBlock::Create(head->getContext(), "else_block", head->getParent());
            if (_has_case_expr) {
                LLVMDatum datum_0(b);
                for (size_t i = 0; i + 1 < _children.size(); i += (1 + (i > 0))) { // 0, 1, 3, 5 ,,,
                    auto* then = llvm::BasicBlock::Create(head->getContext(), "then_" + std::to_string(i),
                                                          head->getParent());
                    auto* next = llvm::BasicBlock::Create(head->getContext(), "next_" + std::to_string(i),
                                                          head->getParent());
                    ASSIGN_OR_RETURN(auto datum_i, _children[i]->generate_ir(context, jit_ctx))
                    if (i == 0) { // if caseExpr is null, go to else
                        datum_0 = datum_i;
                        llvm::Value* is_null = nullptr;
                        if (_children[i]->is_nullable()) {
                            is_null = b.CreateICmpEQ(datum_i.null_flag, llvm::ConstantInt::get(b.getInt8Ty(), 1));
                        } else {
                            is_null = llvm::ConstantInt::get(b.getInt1Ty(), 0);
                        }
                        b.CreateCondBr(is_null, then, next);
                        b.SetInsertPoint(then);
                        b.CreateBr(else_block);
                    } else { // if (whenExpr !=null & caseExpr = whenExpr), store the result
                        llvm::Value* cmp_eq = nullptr;
                        if constexpr (lt_is_float<ResultType>) {
                            cmp_eq = b.CreateFCmpOEQ(datum_0.value, datum_i.value);
                        } else {
                            cmp_eq = b.CreateICmpEQ(datum_0.value, datum_i.value);
                        }
                        if (_children[i]->is_nullable()) {
                            auto* not_null =
                                    b.CreateICmpEQ(datum_i.null_flag, llvm::ConstantInt::get(b.getInt8Ty(), 0));
                            b.CreateCondBr(b.CreateAnd(not_null, cmp_eq), then, next);
                        } else {
                            b.CreateCondBr(cmp_eq, then, next);
                        }
                        b.SetInsertPoint(then);
                        ASSIGN_OR_RETURN(auto datum_i_1, _children[i + 1]->generate_ir(context, jit_ctx))
                        b.CreateStore(datum_i_1.value, res);
                        b.CreateStore(datum_i_1.null_flag, res_null);
                        b.CreateBr(join);
                    }
                    b.SetInsertPoint(next);
                }
            } else {
                for (size_t i = 0; i + 1 < _children.size(); i += 2) {
                    auto* then = llvm::BasicBlock::Create(head->getContext(), "then_" + std::to_string(i),
                                                          head->getParent());
                    auto* next = llvm::BasicBlock::Create(head->getContext(), "next_" + std::to_string(i),
                                                          head->getParent());
                    ASSIGN_OR_RETURN(auto datum_i, _children[i]->generate_ir(context, jit_ctx))
                    auto* is_true = IRHelper::bool_to_cond(b, datum_i.value);
                    if (_children[i]->is_nullable()) {
                        auto* not_null = b.CreateICmpEQ(datum_i.null_flag, llvm::ConstantInt::get(b.getInt8Ty(), 0));
                        b.CreateCondBr(b.CreateAnd(not_null, is_true), then, next);
                    } else {
                        b.CreateCondBr(is_true, then, next);
                    }
                    b.SetInsertPoint(then);

                    ASSIGN_OR_RETURN(auto datum_i_1, _children[i + 1]->generate_ir(context, jit_ctx))
                    b.CreateStore(datum_i_1.value, res);
                    b.CreateStore(datum_i_1.null_flag, res_null);
                    b.CreateBr(join);
                    b.SetInsertPoint(next);
                }
            }
            b.CreateBr(else_block);
            b.SetInsertPoint(else_block);
            LLVMDatum else_val;
            if (_has_else_expr) {
                ASSIGN_OR_RETURN(else_val, _children.back()->generate_ir(context, jit_ctx))
            } else {
                ASSIGN_OR_RETURN(else_val.value, IRHelper::create_ir_number(b, ResultType, 0))
                else_val.null_flag = llvm::ConstantInt::get(b.getInt8Ty(), 1);
            }
            b.CreateStore(else_val.value, res);
            b.CreateStore(else_val.null_flag, res_null);

            b.CreateBr(join);
            b.SetInsertPoint(join);
            result.value = b.CreateLoad(IRHelper::logical_to_ir_type(b, ResultType).value(), res);
            result.null_flag = b.CreateLoad(b.getInt8Ty(), res_null);
            return result;
        } else {
            return Status::NotSupported("JIT of case..when..else...end not support");
        }
    }

    std::string jit_func_name_impl(RuntimeState* state) const override {
        std::stringstream out;
        out << "{";
        for (auto i = 0; i < _children.size(); i++) {
            if (i == 0) {
                if (_has_case_expr) {
                    out << "C";
                } else {
                    out << "CW";
                }
            } else if (i + 1 == _children.size() && _has_else_expr) {
                out << "EL";
            } else {
                if ((i + _has_case_expr) % 2 == 0) {
                    out << "W";
                } else {
                    out << "T";
                }
            }
            out << "<" << _children[i]->jit_func_name(state) << ">";
        }
        out << "}" << (is_constant() ? "c:" : "") << (is_nullable() ? "n:" : "") << type().debug_string();
        return out.str();
    }
#endif

    std::string debug_string() const override {
        std::stringstream out;
        auto expr_debug_string = Expr::debug_string();
        out << "VectorizedCaseWhenExpr ( ";
        for (auto i = 0; i < _children.size(); i++) {
            if (i == 0) {
                if (_has_case_expr) {
                    out << "case";
                } else {
                    out << "case when";
                }
            } else if (i + 1 == _children.size() && _has_else_expr) {
                out << "else";
            } else {
                if ((i + _has_case_expr) % 2 == 0) {
                    out << "when";
                } else {
                    out << "then";
                }
            }
            out << "<" << _children[i]->type().debug_string() << " nullable " << _children[i]->is_nullable()
                << " const=" << _children[i]->is_constant() << "> ";
        }
        out << " result=" << this->type().debug_string() << ", expr (" << expr_debug_string << ") )";
        return out.str();
    }

    StatusOr<ColumnPtr> evaluate_checked(ExprContext* context, Chunk* chunk) override {
        if (_has_case_expr) {
            return evaluate_case(context, chunk);
        } else {
            return evaluate_no_case(context, chunk);
        }
    }

private:
    // CASE 1:
    //   CASE sex
    //       WHEN '1' THEN 'man'
    //       WHEN '2' THEN 'woman'
    //   ELSE 'other' END
    //
    //   If `CASE` is null, return `ELSE`
    //   If ALL `WHEN` is null, return `ELSE`
    //   If `CASE` equals `WHEN`, return `THEN`
    //   If `CASE` can't match ANY `WHEN`, return `ELSE`
    //
    // CASE 2:
    //   CASE sex
    //       WHEN '1' THEN 'man'
    //       WHEN '2' THEN 'woman'
    //
    //   If `CASE` is null, return NULL
    //   If ALL `WHEN` is null, return NULL
    //   If `CASE` equals `WHEN`, return `THEN`
    //   If `CASE` can't match ANY `WHEN`, return NULL
    StatusOr<ColumnPtr> evaluate_case(ExprContext* context, Chunk* chunk) {
        ColumnPtr else_column = nullptr;
        if (!_has_else_expr) {
            else_column = ColumnHelper::create_const_null_column(chunk != nullptr ? chunk->num_rows() : 1);
        } else {
            ASSIGN_OR_RETURN(else_column, _children[_children.size() - 1]->evaluate_checked(context, chunk));
        }

        ASSIGN_OR_RETURN(ColumnPtr case_column, _children[0]->evaluate_checked(context, chunk));
        if (ColumnHelper::count_nulls(case_column) == case_column->size()) {
            return Column::mutate(std::move(else_column));
        }

        int loop_end = _children.size() - 1;

        Columns when_columns;
        when_columns.reserve(loop_end);

        Columns then_columns;
        then_columns.reserve(loop_end);

        for (int i = 1; i < loop_end; i += 2) {
            ASSIGN_OR_RETURN(ColumnPtr when_column, _children[i]->evaluate_checked(context, chunk));

            // skip if all null
            if (ColumnHelper::count_nulls(when_column) == when_column->size()) {
                continue;
            }

            ASSIGN_OR_RETURN(ColumnPtr then_column, _children[i + 1]->evaluate_checked(context, chunk));

            when_columns.emplace_back(when_column);
            then_columns.emplace_back(then_column);
        }

        if (when_columns.empty()) {
            return Column::mutate(std::move(else_column));
        }
        then_columns.emplace_back(else_column);
        size_t size = when_columns[0]->size();
        if constexpr (lt_is_collection<ResultType> || lt_is_collection<WhenType>) {
            // construct result column
            bool res_nullable = false;
            for (const auto& col : then_columns) {
                if (col->is_nullable() || col->only_null()) {
                    res_nullable = true;
                }
            }
            MutableColumnPtr res = ColumnHelper::create_column(this->type(), res_nullable);

            for (auto& then_column : then_columns) {
                then_column = ColumnHelper::unpack_and_duplicate_const_column(size, then_column);
            }
            for (auto& when_column : when_columns) {
                when_column = ColumnHelper::unpack_and_duplicate_const_column(size, when_column);
            }
            case_column = ColumnHelper::unpack_and_duplicate_const_column(size, case_column);

            // then_columns.size >= when_columns.size as else_column maybe exist.
            auto when_num = when_columns.size();
            NullColumnPtr case_nulls = nullptr;
            if (case_column->is_nullable()) {
                case_nulls = down_cast<NullableColumn*>(case_column.get())->null_column();
            }
            auto case_data = ColumnHelper::get_data_column(case_column.get());

            for (auto row = 0; row < size; ++row) {
                int i = 0;
                while ((i < when_num) && ((case_nulls != nullptr && case_nulls->get_data()[row]) ||
                                          !when_columns[i]->equals(row, *case_data, row))) {
                    ++i;
                }
                if (then_columns[i]->is_null(row)) {
                    res->append_nulls(1);
                } else {
                    res->append(*then_columns[i], row, 1);
                }
            }

            return res;
        } else {
            std::vector<ColumnViewer<WhenType>> when_viewers;
            when_viewers.reserve(loop_end);

            std::vector<ColumnViewer<ResultType>> then_viewers;
            then_viewers.reserve(loop_end);
            for (auto& col : when_columns) {
                when_viewers.emplace_back(col);
            }
            for (auto& col : then_columns) {
                then_viewers.emplace_back(col);
            }
            when_columns.emplace_back(case_column);

            bool when_columns_has_null = false;
            for (ColumnPtr& column : when_columns) {
                when_columns_has_null |= column->has_null();
            }
            ColumnViewer<WhenType> case_viewer(case_column);
            then_viewers.emplace_back(else_column);

            ColumnBuilder<ResultType> builder(size, this->type().precision, this->type().scale);

            size_t view_size = when_viewers.size();
            if (!when_columns_has_null) {
                for (int row = 0; row < size; ++row) {
                    int i = 0;
                    while ((i < view_size) && (when_viewers[i].value(row) != case_viewer.value(row))) {
                        i += 1;
                    }
                    if (!then_viewers[i].is_null(row)) {
                        builder.append(then_viewers[i].value(row));
                    } else {
                        builder.append_null();
                    }
                }
            } else {
                for (int row = 0; row < size; ++row) {
                    int i = view_size;
                    if (!case_viewer.is_null(row)) {
                        i = 0;
                        while ((i < view_size) &&
                               (when_viewers[i].is_null(row) || when_viewers[i].value(row) != case_viewer.value(row))) {
                            i += 1;
                        }
                    }
                    if (!then_viewers[i].is_null(row)) {
                        builder.append(then_viewers[i].value(row));
                    } else {
                        builder.append_null();
                    }
                }
            }

            return builder.build(ColumnHelper::is_all_const(when_columns) && ColumnHelper::is_all_const(then_columns));
        }
    }

    // CASE 1:
    //    CASE WHEN sex = '1' THEN 'man'
    //         WHEN sex = '2' THEN 'woman'
    //    ELSE 'other' END
    //
    //  Special CASE-WHEN statement, and `WHEN` clause must be boolean.
    //  If all `WHEN` is null/false, return `ELSE`
    //  If `WHEN` is not null and true, return `THEN`
    //
    // CASE 2:
    //    CASE WHEN sex = '1' THEN 'man'
    //         WHEN sex = '2' THEN 'woman'
    //
    //  Special CASE-WHEN statement, and `WHEN` clause must be boolean.
    //  If all `WHEN` is null/false, return NULL
    //  If `WHEN` is not null and true, return `THEN`
    StatusOr<ColumnPtr> evaluate_no_case(ExprContext* context, Chunk* chunk) {
        ColumnPtr else_column = nullptr;
        if (!_has_else_expr) {
            else_column = ColumnHelper::create_const_null_column(chunk != nullptr ? chunk->num_rows() : 1);
        } else {
            ASSIGN_OR_RETURN(else_column, _children[_children.size() - 1]->evaluate_checked(context, chunk));
        }

        int loop_end = _children.size() - 1;

        Columns when_columns;
        when_columns.reserve(loop_end);

        Columns then_columns;
        then_columns.reserve(loop_end);

        std::vector<ColumnViewer<TYPE_BOOLEAN>> when_viewers;
        when_viewers.reserve(loop_end);

        for (int i = 0; i < loop_end; i += 2) {
            ASSIGN_OR_RETURN(ColumnPtr when_column, _children[i]->evaluate_checked(context, chunk));

            size_t trues_count = ColumnHelper::count_true_with_notnull(when_column);

            // skip if all false or all null
            if (trues_count == 0) {
                continue;
            }

            ASSIGN_OR_RETURN(ColumnPtr then_column, _children[i + 1]->evaluate_checked(context, chunk));

            // direct return if first when is all true
            if (when_viewers.empty() && trues_count == when_column->size()) {
                return Column::mutate(std::move(then_column));
            }

            when_columns.emplace_back(when_column);
            then_columns.emplace_back(then_column);
            when_viewers.emplace_back(when_column);
        }

        if (when_viewers.empty()) {
            return Column::mutate(std::move(else_column));
        }
        then_columns.emplace_back(else_column);

        size_t size = when_columns[0]->size();

        bool when_columns_has_null = false;
        for (ColumnPtr& column : when_columns) {
            when_columns_has_null |= column->has_null();
        }

        if constexpr (lt_is_collection<ResultType>) {
            // construct nullable result column
            bool res_nullable = false;
            for (const auto& col : then_columns) {
                if (col->is_nullable() || col->only_null()) {
                    res_nullable = true;
                }
            }
            MutableColumnPtr res = ColumnHelper::create_column(this->type(), res_nullable);

            for (auto& then_column : then_columns) {
                then_column = ColumnHelper::unpack_and_duplicate_const_column(size, then_column);
            }
            // when_columns[i] is true or not
            auto when_num = when_columns.size();
            if (!when_columns_has_null) {
                for (auto row = 0; row < size; ++row) {
                    int i = 0;
                    while (i < when_num && !(when_viewers[i].value(row))) {
                        ++i;
                    }
                    if (then_columns[i]->is_null(row)) {
                        res->append_nulls(1);
                    } else {
                        res->append(*then_columns[i], row, 1);
                    }
                }
            } else {
                for (auto row = 0; row < size; ++row) {
                    int i = 0;
                    while ((i < when_num) && (when_viewers[i].is_null(row) || !when_viewers[i].value(row))) {
                        ++i;
                    }
                    if (then_columns[i]->is_null(row)) {
                        res->append_nulls(1);
                    } else {
                        res->append(*then_columns[i], row, 1);
                    }
                }
            }
            return res;
        } else {
            std::vector<ColumnViewer<ResultType>> then_viewers;
            then_viewers.reserve(loop_end);
            for (auto& col : then_columns) {
                then_viewers.emplace_back(col);
            }
            ColumnBuilder<ResultType> builder(size, this->type().precision, this->type().scale);
            // max case size in use SIMD CASE WHEN implements
            constexpr int max_simd_case_when_size = 8;

            // optimization for no-nullable Arithmetic Type
            if constexpr (isArithmeticLT<ResultType>) {
                bool then_columns_has_null = false;
                for (const auto& column : then_columns) {
                    then_columns_has_null |= column->has_null();
                }

                bool check_could_use_multi_simd_selector = !when_columns_has_null &&
                                                           when_columns.size() <= max_simd_case_when_size &&
                                                           !then_columns_has_null;

                if (check_could_use_multi_simd_selector) {
                    int then_column_size = then_columns.size();
                    int when_column_size = when_columns.size();
                    std::vector<bool> then_column_is_const(then_column_size);
                    for (int i = 0; i < then_column_size; ++i) {
                        then_column_is_const[i] = then_columns[i]->is_constant();
                    }
                    for (int i = 0; i < when_column_size; ++i) {
                        when_columns[i] = ColumnHelper::unpack_and_duplicate_const_column(size, when_columns[i]);
                    }
                    for (int i = 0; i < when_column_size; ++i) {
                        ColumnHelper::merge_nullable_filter(when_columns[i].get());
                    }

                    using ResultContainer = typename RunTimeColumnType<ResultType>::Container;

                    ResultContainer* select_list[then_column_size];
                    for (int i = 0; i < then_column_size; ++i) {
                        auto* data_column = ColumnHelper::get_data_column(then_columns[i].get());
                        select_list[i] = &down_cast<RunTimeColumnType<ResultType>*>(data_column)->get_data();
                    }

                    uint8_t* select_vec[when_column_size];
                    for (int i = 0; i < when_column_size; ++i) {
                        auto* data_column = ColumnHelper::get_data_column(when_columns[i].get());
                        select_vec[i] = down_cast<BooleanColumn*>(data_column)->get_data().data();
                    }

                    auto res = RunTimeColumnType<ResultType>::create();

                    if constexpr (lt_is_decimal<ResultType>) {
                        res->set_scale(this->type().scale);
                        res->set_precision(this->type().precision);
                    }

                    auto& container = res->get_data();
                    container.resize(size);
                    SIMD_muti_selector<ResultType>::multi_select_if(select_vec, when_column_size, container,
                                                                    select_list, then_column_size, then_column_is_const,
                                                                    size);
                    return res;
                }
            }

            size_t view_size = when_viewers.size();
            if (!when_columns_has_null) {
                for (int row = 0; row < size; ++row) {
                    int i = 0;
                    while (i < view_size && !(when_viewers[i].value(row))) {
                        i += 1;
                    }
                    if (!then_viewers[i].is_null(row)) {
                        builder.append(then_viewers[i].value(row));
                    } else {
                        builder.append_null();
                    }
                }
            } else {
                for (int row = 0; row < size; ++row) {
                    int i = 0;
                    while ((i < view_size) && (when_viewers[i].is_null(row) || !when_viewers[i].value(row))) {
                        i += 1;
                    }

                    if (!then_viewers[i].is_null(row)) {
                        builder.append(then_viewers[i].value(row));
                    } else {
                        builder.append_null();
                    }
                }
            }

            return builder.build(ColumnHelper::is_all_const(when_columns) && ColumnHelper::is_all_const(then_columns));
        }
    }

private:
    const bool _has_case_expr;
    const bool _has_else_expr;
};

#define CASE_WHEN_RESULT_TYPE(WHEN_TYPE, RESULT_TYPE)                \
    case WHEN_TYPE: {                                                \
        return new VectorizedCaseExpr<WHEN_TYPE, RESULT_TYPE>(node); \
    }

#define SWITCH_ALL_WHEN_TYPE(RESULT_TYPE)                                                 \
    switch (whenType) {                                                                   \
        CASE_WHEN_RESULT_TYPE(TYPE_BOOLEAN, RESULT_TYPE);                                 \
        CASE_WHEN_RESULT_TYPE(TYPE_TINYINT, RESULT_TYPE);                                 \
        CASE_WHEN_RESULT_TYPE(TYPE_SMALLINT, RESULT_TYPE);                                \
        CASE_WHEN_RESULT_TYPE(TYPE_INT, RESULT_TYPE);                                     \
        CASE_WHEN_RESULT_TYPE(TYPE_BIGINT, RESULT_TYPE);                                  \
        CASE_WHEN_RESULT_TYPE(TYPE_LARGEINT, RESULT_TYPE);                                \
        CASE_WHEN_RESULT_TYPE(TYPE_FLOAT, RESULT_TYPE);                                   \
        CASE_WHEN_RESULT_TYPE(TYPE_DOUBLE, RESULT_TYPE);                                  \
        CASE_WHEN_RESULT_TYPE(TYPE_CHAR, RESULT_TYPE);                                    \
        CASE_WHEN_RESULT_TYPE(TYPE_VARCHAR, RESULT_TYPE);                                 \
        CASE_WHEN_RESULT_TYPE(TYPE_DATE, RESULT_TYPE);                                    \
        CASE_WHEN_RESULT_TYPE(TYPE_DATETIME, RESULT_TYPE);                                \
        CASE_WHEN_RESULT_TYPE(TYPE_TIME, RESULT_TYPE);                                    \
        CASE_WHEN_RESULT_TYPE(TYPE_DECIMALV2, RESULT_TYPE);                               \
        CASE_WHEN_RESULT_TYPE(TYPE_DECIMAL32, RESULT_TYPE);                               \
        CASE_WHEN_RESULT_TYPE(TYPE_DECIMAL64, RESULT_TYPE);                               \
        CASE_WHEN_RESULT_TYPE(TYPE_DECIMAL128, RESULT_TYPE);                              \
        CASE_WHEN_RESULT_TYPE(TYPE_DECIMAL256, RESULT_TYPE);                              \
        CASE_WHEN_RESULT_TYPE(TYPE_JSON, RESULT_TYPE);                                    \
        CASE_WHEN_RESULT_TYPE(TYPE_ARRAY, RESULT_TYPE);                                   \
        CASE_WHEN_RESULT_TYPE(TYPE_MAP, RESULT_TYPE);                                     \
        CASE_WHEN_RESULT_TYPE(TYPE_STRUCT, RESULT_TYPE);                                  \
    default: {                                                                            \
        LOG(WARNING) << "vectorized engine case expr no support when type: " << whenType; \
        return nullptr;                                                                   \
    }                                                                                     \
    }

#define CASE_RESULT_TYPE(RESULT_TYPE)      \
    case RESULT_TYPE: {                    \
        SWITCH_ALL_WHEN_TYPE(RESULT_TYPE); \
    }

Expr* VectorizedCaseExprFactory::from_thrift(const starrocks::TExprNode& node) {
    LogicalType resultType = TypeDescriptor::from_thrift(node.type).type;
    LogicalType whenType = thrift_to_type(node.child_type);

    if (resultType == TYPE_NULL) {
        resultType = TYPE_BOOLEAN;
    }

    switch (resultType) {
        APPLY_FOR_ALL_SCALAR_TYPE(CASE_RESULT_TYPE)
        APPLY_FOR_COMPLEX_TYPE(CASE_RESULT_TYPE)
        CASE_RESULT_TYPE(TYPE_OBJECT)
        CASE_RESULT_TYPE(TYPE_HLL)
        CASE_RESULT_TYPE(TYPE_PERCENTILE)
    default: {
        LOG(WARNING) << "vectorized engine case expr no support result type: " << resultType;
        return nullptr;
    }
    }
}

Expr* VectorizedCaseExprFactory::from_thrift(const starrocks::TExprNode& node, LogicalType resultType,
                                             LogicalType whenType) {
    if (resultType == TYPE_NULL) {
        resultType = TYPE_BOOLEAN;
    }

    switch (resultType) {
        APPLY_FOR_ALL_SCALAR_TYPE(CASE_RESULT_TYPE)
        APPLY_FOR_COMPLEX_TYPE(CASE_RESULT_TYPE)
        CASE_RESULT_TYPE(TYPE_OBJECT)
        CASE_RESULT_TYPE(TYPE_HLL)
        CASE_RESULT_TYPE(TYPE_PERCENTILE)
    default: {
        LOG(WARNING) << "vectorized engine case expr no support result type: " << resultType;
        return nullptr;
    }
    }
}

#undef CASE_WHEN_RESULT_TYPE
#undef SWITCH_ALL_WHEN_TYPE
#undef CASE_RESULT_TYPE

} // namespace starrocks
