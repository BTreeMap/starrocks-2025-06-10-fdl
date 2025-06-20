#include "runtime/fragment_mgr.h"

#include <gtest/gtest.h>

#include <utility>

#include "common/config.h"
#include "exec/data_sink.h"
#include "runtime/exec_env.h"
#include "runtime/plan_fragment_executor.h"
#include "util/monotime.h"

namespace starrocks {

// Mock used for this unittest
PlanFragmentExecutor::PlanFragmentExecutor(ExecEnv* exec_env, report_status_callback report_status_cb)
        : _exec_env(exec_env), _report_status_cb(std::move(report_status_cb)) {}

PlanFragmentExecutor::~PlanFragmentExecutor() = default;

Status PlanFragmentExecutor::prepare(const TExecPlanFragmentParams& request) {
    return Status::OK();
}

Status PlanFragmentExecutor::open() {
    SleepFor(MonoDelta::FromMilliseconds(50));
    return Status::OK();
}

void PlanFragmentExecutor::cancel() {}

void PlanFragmentExecutor::close() {}

void PlanFragmentExecutor::report_profile_once() {}

class FragmentMgrTest : public testing::Test {
public:
    FragmentMgrTest() = default;

protected:
    void SetUp() override {
        config::fragment_pool_thread_num_min = 32;
        config::fragment_pool_thread_num_max = 32;
        config::fragment_pool_queue_size = 1024;
    }
    void TearDown() override {}
};

TEST_F(FragmentMgrTest, Normal) {
    FragmentMgr mgr(ExecEnv::GetInstance());
    TExecPlanFragmentParams params;
    params.params.fragment_instance_id = TUniqueId();
    params.params.fragment_instance_id.__set_hi(100);
    params.params.fragment_instance_id.__set_lo(200);
    ASSERT_TRUE(mgr.exec_plan_fragment(params).ok());
    // Duplicated
    ASSERT_TRUE(mgr.exec_plan_fragment(params).ok());
}

TEST_F(FragmentMgrTest, AddNormal) {
    FragmentMgr mgr(ExecEnv::GetInstance());
    for (int i = 0; i < 8; ++i) {
        TExecPlanFragmentParams params;
        params.params.fragment_instance_id = TUniqueId();
        params.params.fragment_instance_id.__set_hi(100 + i);
        params.params.fragment_instance_id.__set_lo(200);
        ASSERT_TRUE(mgr.exec_plan_fragment(params).ok());
    }
}

TEST_F(FragmentMgrTest, CancelNormal) {
    FragmentMgr mgr(ExecEnv::GetInstance());
    TExecPlanFragmentParams params;
    params.params.fragment_instance_id = TUniqueId();
    params.params.fragment_instance_id.__set_hi(100);
    params.params.fragment_instance_id.__set_lo(200);
    ASSERT_TRUE(mgr.exec_plan_fragment(params).ok());
    // Cancel after add
    ASSERT_TRUE(mgr.cancel(params.params.fragment_instance_id).ok());
}

TEST_F(FragmentMgrTest, CloseNornaml) {
    FragmentMgr mgr(ExecEnv::GetInstance());
    TExecPlanFragmentParams params;
    params.params.fragment_instance_id = TUniqueId();
    params.params.fragment_instance_id.__set_hi(100);
    params.params.fragment_instance_id.__set_lo(200);
    ASSERT_TRUE(mgr.exec_plan_fragment(params).ok());

    // Close after add, no dead lock
    mgr.close();

    // error when adding fragment after close()
    {
        TExecPlanFragmentParams params;
        params.params.fragment_instance_id = TUniqueId();
        params.params.fragment_instance_id.__set_hi(200);
        params.params.fragment_instance_id.__set_lo(300);
        auto st = mgr.exec_plan_fragment(params);
        EXPECT_FALSE(st.ok());
        EXPECT_TRUE(st.is_cancelled()) << "exec_plan_fragment() failed with error:" << st;
    }
}

TEST_F(FragmentMgrTest, CancelWithoutAdd) {
    FragmentMgr mgr(ExecEnv::GetInstance());
    TExecPlanFragmentParams params;
    params.params.fragment_instance_id = TUniqueId();
    params.params.fragment_instance_id.__set_hi(100);
    params.params.fragment_instance_id.__set_lo(200);
    ASSERT_TRUE(mgr.cancel(params.params.fragment_instance_id).ok());
}

} // namespace starrocks
