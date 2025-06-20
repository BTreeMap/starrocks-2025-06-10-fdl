#pragma once
namespace starrocks {

// For now, these are simply ints; if we find we need to generate ids in the
// backend, we can also introduce separate classes for these to make them
// assignment-incompatible.
typedef int TupleId;
typedef int SlotId;
typedef int TableId;
typedef int PlanNodeId;

// Mapping from input slot to output slot of an ExecNode.
// It is used for pipeline to rewrite runtime in filters.
struct TupleSlotMapping {
    TupleId from_tuple_id;
    SlotId from_slot_id;
    TupleId to_tuple_id;
    SlotId to_slot_id;

    TupleSlotMapping() = default;
    ~TupleSlotMapping() = default;

    TupleSlotMapping(const TupleSlotMapping&) = default;
    TupleSlotMapping(TupleSlotMapping&&) = default;
    TupleSlotMapping& operator=(TupleSlotMapping&&) = default;
    TupleSlotMapping& operator=(const TupleSlotMapping&) = default;

    TupleSlotMapping(TupleId from_tuple_id, SlotId from_slot_id, TupleId to_tuple_id, SlotId to_slot_id)
            : from_tuple_id(from_tuple_id),
              from_slot_id(from_slot_id),
              to_tuple_id(to_tuple_id),
              to_slot_id(to_slot_id) {}
};

}; // namespace starrocks
