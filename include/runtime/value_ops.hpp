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
  inline auto value_equals(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> bool;
  inline auto value_equals(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool;

  inline auto deref(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
  {
    return auto_deref_value(value);
  }

  inline auto deref(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<NGObject>
  {
    return deref(cell ? cell->boxedValue : nullptr);
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

  inline auto numeric_order(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> Orders
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs);
    auto rightNum = std::dynamic_pointer_cast<NumeralBase>(rhs);
    if (!leftNum || !rightNum)
    {
      return Orders::UNORDERED;
    }
    return lhs->compareTo(rhs.get());
  }

  inline auto value_equals(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> bool
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (lhs.get() == rhs.get())
    {
      return true;
    }
    if (!lhs || !rhs)
    {
      return false;
    }
    if (auto leftBool = std::dynamic_pointer_cast<NGBoolean>(lhs))
    {
      auto rightBool = std::dynamic_pointer_cast<NGBoolean>(rhs);
      return rightBool && leftBool->value == rightBool->value;
    }
    if (auto leftString = std::dynamic_pointer_cast<NGString>(lhs))
    {
      auto rightString = std::dynamic_pointer_cast<NGString>(rhs);
      return rightString && leftString->payload_value() == rightString->payload_value();
    }
    if (auto leftUnit = std::dynamic_pointer_cast<NGUnit>(lhs))
    {
      return std::dynamic_pointer_cast<NGUnit>(rhs) != nullptr;
    }
    if (std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return numeric_order(lhs, rhs) == Orders::EQ;
    }
    if (auto leftArray = std::dynamic_pointer_cast<NGArray>(lhs))
    {
      auto rightArray = std::dynamic_pointer_cast<NGArray>(rhs);
      if (!rightArray || array_length(*leftArray) != array_length(*rightArray))
      {
        return false;
      }
      return aggregate_slots_equal(array_length(*leftArray),
                                   [leftArray](size_t i) { return array_element_slot(*leftArray, i); },
                                   [rightArray](size_t i) { return array_element_slot(*rightArray, i); });
    }
    if (auto leftTuple = std::dynamic_pointer_cast<NGTuple>(lhs))
    {
      auto rightTuple = std::dynamic_pointer_cast<NGTuple>(rhs);
      if (!rightTuple || tuple_length(*leftTuple) != tuple_length(*rightTuple))
      {
        return false;
      }
      return aggregate_slots_equal(tuple_length(*leftTuple),
                                   [leftTuple](size_t i) { return tuple_element_slot(*leftTuple, i); },
                                   [rightTuple](size_t i) { return tuple_element_slot(*rightTuple, i); });
    }
    if (auto leftTagged = std::dynamic_pointer_cast<NGTaggedValue>(lhs))
    {
      auto rightTagged = std::dynamic_pointer_cast<NGTaggedValue>(rhs);
      if (!rightTagged || leftTagged->unionName != rightTagged->unionName ||
          leftTagged->variantIndex != rightTagged->variantIndex ||
          leftTagged->payload_items().size() != rightTagged->payload_items().size())
      {
        return false;
      }
      return aggregate_slots_equal(leftTagged->payload_items().size(),
                                   [leftTagged](size_t i) { return leftTagged->payload_slot(i); },
                                   [rightTagged](size_t i) { return rightTagged->payload_slot(i); });
    }
    if (auto leftNewType = std::dynamic_pointer_cast<NGNewType>(lhs))
    {
      auto rightNewType = std::dynamic_pointer_cast<NGNewType>(rhs);
      return rightNewType && leftNewType->newType->name == rightNewType->newType->name &&
             value_equals(leftNewType->wrapped, rightNewType->wrapped);
    }
    return false;
  }

  inline auto value_equals(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    return value_equals(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_less_than(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> bool
  {
    if (auto order = numeric_order(left, right); order != Orders::UNORDERED)
    {
      return order == Orders::LT;
    }
    if (auto lhs = std::dynamic_pointer_cast<NGString>(deref(left)))
    {
      auto rhs = std::dynamic_pointer_cast<NGString>(deref(right));
      return rhs && lhs->payload_value() < rhs->payload_value();
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_less_than(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    return value_less_than(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_greater_than(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> bool
  {
    if (auto order = numeric_order(left, right); order != Orders::UNORDERED)
    {
      return order == Orders::GT;
    }
    if (auto lhs = std::dynamic_pointer_cast<NGString>(deref(left)))
    {
      auto rhs = std::dynamic_pointer_cast<NGString>(deref(right));
      return rhs && lhs->payload_value() > rhs->payload_value();
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_greater_than(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> bool
  {
    return value_greater_than(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_add(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return lhs->opPlus(rhs);
    }
    if (auto leftString = std::dynamic_pointer_cast<NGString>(lhs))
    {
      return makert<NGString>(leftString->payload_value() + runtime_value_show(rhs));
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_add(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right) -> RuntimeRef<NGObject>
  {
    return value_add(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_subtract(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return lhs->opMinus(rhs);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_subtract(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<NGObject>
  {
    return value_subtract(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_multiply(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return lhs->opTimes(rhs);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_multiply(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<NGObject>
  {
    return value_multiply(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_divide(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return lhs->opDividedBy(rhs);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_divide(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<NGObject>
  {
    return value_divide(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_modulus(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    auto rhs = deref(right);
    if (auto leftNum = std::dynamic_pointer_cast<NumeralBase>(lhs))
    {
      return lhs->opModulus(rhs);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_modulus(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<NGObject>
  {
    return value_modulus(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_lshift(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    if (auto leftArray = std::dynamic_pointer_cast<NGArray>(lhs))
    {
      return leftArray->opLShift(right);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_lshift(const RuntimeRef<StorageCell> &left, const RuntimeRef<StorageCell> &right)
      -> RuntimeRef<NGObject>
  {
    return value_lshift(left ? left->boxedValue : nullptr, right ? right->boxedValue : nullptr);
  }

  inline auto value_rshift(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    throw RuntimeException("Unsupported binary operator");
  }
} // namespace NG::runtime::ops
