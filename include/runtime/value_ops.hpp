#pragma once

#include <runtime/array_layout_access.hpp>
#include <runtime/value_access.hpp>
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>

namespace NG::runtime::ops
{
  inline auto value_equals(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> bool;

  inline auto deref(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
  {
    return auto_deref_value(value);
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
      auto leftItems = leftArray->payload_items();
      auto rightItems = rightArray ? rightArray->payload_items() : Vec<RuntimeRef<NGObject>>{};
      if (!rightArray || leftItems.size() != rightItems.size())
      {
        return false;
      }
      for (size_t i = 0; i < leftItems.size(); ++i)
      {
        if (!value_equals(leftItems[i], rightItems[i]))
        {
          return false;
        }
      }
      return true;
    }
    if (auto leftTuple = std::dynamic_pointer_cast<NGTuple>(lhs))
    {
      auto rightTuple = std::dynamic_pointer_cast<NGTuple>(rhs);
      auto leftItems = leftTuple->payload_items();
      auto rightItems = rightTuple ? rightTuple->payload_items() : Vec<RuntimeRef<NGObject>>{};
      if (!rightTuple || leftItems.size() != rightItems.size())
      {
        return false;
      }
      for (size_t i = 0; i < leftItems.size(); ++i)
      {
        if (!value_equals(leftItems[i], rightItems[i]))
        {
          return false;
        }
      }
      return true;
    }
    if (auto leftTagged = std::dynamic_pointer_cast<NGTaggedValue>(lhs))
    {
      auto rightTagged = std::dynamic_pointer_cast<NGTaggedValue>(rhs);
      auto leftPayload = leftTagged->payload_items();
      auto rightPayload = rightTagged ? rightTagged->payload_items() : Vec<RuntimeRef<NGObject>>{};
      if (!rightTagged || leftTagged->unionName != rightTagged->unionName ||
          leftTagged->variantIndex != rightTagged->variantIndex ||
          leftPayload.size() != rightPayload.size())
      {
        return false;
      }
      for (size_t i = 0; i < leftPayload.size(); ++i)
      {
        if (!value_equals(leftPayload[i], rightPayload[i]))
        {
          return false;
        }
      }
      return true;
    }
    if (auto leftNewType = std::dynamic_pointer_cast<NGNewType>(lhs))
    {
      auto rightNewType = std::dynamic_pointer_cast<NGNewType>(rhs);
      return rightNewType && leftNewType->newType->name == rightNewType->newType->name &&
             value_equals(leftNewType->wrapped, rightNewType->wrapped);
    }
    return false;
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

  inline auto value_lshift(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    auto lhs = deref(left);
    if (auto leftArray = std::dynamic_pointer_cast<NGArray>(lhs))
    {
      return leftArray->opLShift(right);
    }
    throw RuntimeException("Unsupported binary operator");
  }

  inline auto value_rshift(const RuntimeRef<NGObject> &left, const RuntimeRef<NGObject> &right) -> RuntimeRef<NGObject>
  {
    throw RuntimeException("Unsupported binary operator");
  }
} // namespace NG::runtime::ops
