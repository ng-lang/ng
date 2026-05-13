#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/tagged_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime::ops
{
  inline auto value_equals(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool;
  inline auto value_order(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> Orders;

  inline auto is_nominal_wrapper_cell(const RuntimeRef<StorageCell> &cell) -> bool;

  inline auto dispatch_binary_operator(const RuntimeRef<StorageCell> &left, RuntimeBinaryOperator op,
                                       const RuntimeRef<StorageCell> &right) -> RuntimeRef<StorageCell>
  {
    if (!left || !right)
    {
      return nullptr;
    }
    auto leftType = runtime_value_type(left);
    auto rightType = runtime_value_type(right);
    auto leftSize = runtime_value_layout(left).size;
    auto rightSize = runtime_value_layout(right).size;
    if (rightType && rightSize > leftSize && rightType->cellBinaryOperators.contains(op))
    {
      return rightType->cellBinaryOperators.at(op)(right, left);
    }
    if (!leftType || !leftType->cellBinaryOperators.contains(op))
    {
      if (leftType && rightType && leftType == rightType && is_nominal_wrapper_cell(left) && is_nominal_wrapper_cell(right))
      {
        return dispatch_binary_operator(runtime_cell_slot_ref(left, 0), op, runtime_cell_slot_ref(right, 0));
      }
      return nullptr;
    }
    return leftType->cellBinaryOperators.at(op)(left, right);
  }

  inline auto aggregate_slots_equal(size_t size, auto &&leftSlotAt, auto &&rightSlotAt) -> bool
  {
    for (size_t i = 0; i < size; ++i)
    {
      if (!value_equals(leftSlotAt(i), rightSlotAt(i)))
      {
        return false;
      }
    }
    return true;
  }

  inline auto aggregate_slots_equal(const Vec<RuntimeRef<StorageCell>> &leftSlots,
                                    const Vec<RuntimeRef<StorageCell>> &rightSlots) -> bool
  {
    if (leftSlots.size() != rightSlots.size())
    {
      return false;
    }
    return aggregate_slots_equal(leftSlots.size(),
                                 [&](size_t index) { return leftSlots[index]; },
                                 [&](size_t index) { return rightSlots[index]; });
  }

  inline auto is_nominal_wrapper_cell(const RuntimeRef<StorageCell> &cell) -> bool
  {
    auto type = runtime_value_type(cell);
    return cell && type && type->name != "ref" && type->name != "Array" && type->name != "Tuple" &&
           type->layout.kind != LayoutKind::TAGGED_UNION && type->properties.empty() && cell->opaqueRefs.size() == 1 &&
           cell->namedRefs.empty();
  }

  inline auto value_order(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> Orders
  {
    if (!left || !right)
    {
      return Orders::UNORDERED;
    }
    auto leftType = runtime_value_type(left);
    auto rightType = runtime_value_type(right);
    auto leftSize = runtime_value_layout(left).size;
    auto rightSize = runtime_value_layout(right).size;
    if (rightType && rightSize > leftSize && rightType->cellOrderHandler)
    {
      return negate(rightType->cellOrderHandler(right, left));
    }
    if (!leftType || !leftType->cellOrderHandler)
    {
      if (leftType && rightType && leftType == rightType && is_nominal_wrapper_cell(left) && is_nominal_wrapper_cell(right))
      {
        return value_order(runtime_cell_slot_ref(left, 0), runtime_cell_slot_ref(right, 0));
      }
      return Orders::UNORDERED;
    }
    return leftType->cellOrderHandler(left, right);
  }

  inline auto value_equals(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    if (left.get() == right.get())
    {
      return true;
    }
    if (auto order = value_order(left, right); order != Orders::UNORDERED)
    {
      return order == Orders::EQ;
    }
    auto leftType = runtime_value_type(left);
    auto rightType = runtime_value_type(right);
    if (leftType && rightType && leftType->name == "Array" && rightType->name == "Array")
    {
      auto leftSlots = runtime_cell_slot_refs(left);
      auto rightSlots = runtime_cell_slot_refs(right);
      return aggregate_slots_equal(leftSlots, rightSlots);
    }
    if (leftType && rightType && leftType->name == "Tuple" && rightType->name == "Tuple")
    {
      auto leftSlots = runtime_cell_slot_refs(left);
      auto rightSlots = runtime_cell_slot_refs(right);
      return aggregate_slots_equal(leftSlots, rightSlots);
    }
    if (leftType && rightType && leftType->layout.kind == LayoutKind::TAGGED_UNION &&
        rightType->layout.kind == LayoutKind::TAGGED_UNION &&
        leftType->name == rightType->name &&
        leftType->variantIndex == rightType->variantIndex)
    {
      auto leftSlots = runtime_cell_slot_refs(left);
      auto rightSlots = runtime_cell_slot_refs(right);
      return aggregate_slots_equal(leftSlots, rightSlots);
    }
    if (leftType && rightType && !leftType->properties.empty() && leftType == rightType)
    {
      auto leftSlots = runtime_cell_slot_refs(left);
      auto rightSlots = runtime_cell_slot_refs(right);
      if (!aggregate_slots_equal(leftSlots, rightSlots))
      {
        return false;
      }
      auto leftNamed = runtime_cell_named_slot_refs(left);
      auto rightNamed = runtime_cell_named_slot_refs(right);
      if (leftNamed.size() != rightNamed.size())
      {
        return false;
      }
      for (const auto &[name, slot] : leftNamed)
      {
        if (!rightNamed.contains(name) || !value_equals(slot, rightNamed.at(name)))
        {
          return false;
        }
      }
      return true;
    }
    if (leftType && rightType && leftType == rightType && is_nominal_wrapper_cell(left) && is_nominal_wrapper_cell(right))
    {
      return value_equals(runtime_cell_slot_ref(left, 0), runtime_cell_slot_ref(right, 0));
    }
    return false;
  }

  inline auto value_less_than(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    if (auto order = value_order(left, right); order != Orders::UNORDERED)
    {
      return order == Orders::LT;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_greater_than(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    if (auto order = value_order(left, right); order != Orders::UNORDERED)
    {
      return order == Orders::GT;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_add(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::Add, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_subtract(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::Subtract, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_multiply(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::Multiply, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_divide(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::Divide, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_modulus(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::Modulus, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_lshift(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::LShift, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_rshift(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<StorageCell>
  {
    if (auto result = dispatch_binary_operator(left, RuntimeBinaryOperator::RShift, right))
    {
      return result;
    }
    throw RuntimeException("Unsupported binary operator");
  }
} // namespace NG::runtime::ops
