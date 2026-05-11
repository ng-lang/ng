#pragma once

#include <intp/runtime.hpp>

#include <optional>
#include <variant>

namespace NG::runtime::native
{
  inline auto native_arg_slots_context_key() -> const Str &
  {
    static const Str key = "$$native_arg_slots$$";
    return key;
  }

  struct NativeArgsView
  {
    const NGArgs *values = nullptr;
    std::shared_ptr<Vec<RuntimeRef<StorageCell>>> slotOwner;

    [[nodiscard]] auto size() const -> size_t
    {
      if (slotOwner)
      {
        return slotOwner->size();
      }
      return values ? values->size() : 0;
    }

    [[nodiscard]] auto value_at(size_t index) const -> RuntimeRef<NGObject>
    {
      if (slotOwner && index < slotOwner->size())
      {
        auto slot = (*slotOwner)[index];
        return slot ? slot->boxedValue : nullptr;
      }
      if (values && index < values->size())
      {
        return (*values)[index];
      }
      return nullptr;
    }

    [[nodiscard]] auto slot_at(size_t index) const -> RuntimeRef<StorageCell>
    {
      if (!slotOwner || index >= slotOwner->size())
      {
        return nullptr;
      }
      return (*slotOwner)[index];
    }
  };

  inline auto make_native_args_view(const NGArgs &args) -> NativeArgsView
  {
    return NativeArgsView{.values = &args};
  }

  inline auto current_native_arg_slots(const NGCtx &context) -> std::shared_ptr<Vec<RuntimeRef<StorageCell>>>
  {
    if (!context)
    {
      return nullptr;
    }
    auto state = context->get_runtime_state(native_arg_slots_context_key());
    return state ? std::static_pointer_cast<Vec<RuntimeRef<StorageCell>>>(state) : nullptr;
  }

  inline auto native_args_view(const NGCtx &context, const NGArgs &args) -> NativeArgsView
  {
    return NativeArgsView{
        .values = &args,
        .slotOwner = current_native_arg_slots(context),
    };
  }

  inline auto bind_native_arg_slots(const NGCtx &context, const NGArgs &args) -> std::shared_ptr<Vec<RuntimeRef<StorageCell>>>
  {
    auto slots = std::make_shared<Vec<RuntimeRef<StorageCell>>>();
    slots->reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i)
    {
      auto slot = make_boxed_storage_cell(args[i], StorageClass::TEMPORARY);
      slot->name = "arg." + std::to_string(i);
      slot->ownerContext = context.get();
      slots->push_back(slot);
    }
    if (context)
    {
      context->set_runtime_state(native_arg_slots_context_key(), slots);
    }
    return slots;
  }

  inline void require_arg_count(const Str &functionName, const NativeArgsView &args, size_t minCount,
                                std::optional<size_t> maxCount = std::nullopt)
  {
    if (args.size() < minCount)
    {
      throw RuntimeException(functionName + "() requires at least " + std::to_string(minCount) + " argument(s)");
    }
    if (maxCount.has_value() && args.size() > *maxCount)
    {
      throw RuntimeException(functionName + "() accepts at most " + std::to_string(*maxCount) + " argument(s)");
    }
  }

  inline void require_arg_count(const Str &functionName, const NGArgs &args, size_t minCount,
                                std::optional<size_t> maxCount = std::nullopt)
  {
    require_arg_count(functionName, make_native_args_view(args), minCount, maxCount);
  }

  template <class TObject>
  auto require_arg_as(const Str &functionName, const NativeArgsView &args, size_t index, const Str &expectedType)
      -> RuntimeRef<TObject>
  {
    require_arg_count(functionName, args, index + 1);
    auto value = std::dynamic_pointer_cast<TObject>(args.value_at(index));
    if (!value)
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    return value;
  }

  template <class TObject>
  auto require_arg_as(const Str &functionName, const NGArgs &args, size_t index, const Str &expectedType)
      -> RuntimeRef<TObject>
  {
    return require_arg_as<TObject>(functionName, make_native_args_view(args), index, expectedType);
  }

  template <class TResult, class TObject, class... TRest>
  auto require_arg_as_one_of_impl(const RuntimeRef<NGObject> &value) -> std::optional<TResult>
  {
    if (auto typed = std::dynamic_pointer_cast<TObject>(value))
    {
      return TResult{typed};
    }
    if constexpr (sizeof...(TRest) > 0)
    {
      return require_arg_as_one_of_impl<TResult, TRest...>(value);
    }
    return std::nullopt;
  }

  template <class... TObject>
  auto require_arg_as_one_of(const Str &functionName, const NativeArgsView &args, size_t index, const Str &expectedType)
      -> std::variant<RuntimeRef<TObject>...>
  {
    static_assert(sizeof...(TObject) > 0);
    require_arg_count(functionName, args, index + 1);
    using Result = std::variant<RuntimeRef<TObject>...>;
    auto matched = require_arg_as_one_of_impl<Result, TObject...>(args.value_at(index));
    if (!matched)
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    return *matched;
  }

  template <class... TObject>
  auto require_arg_as_one_of(const Str &functionName, const NGArgs &args, size_t index, const Str &expectedType)
      -> std::variant<RuntimeRef<TObject>...>
  {
    return require_arg_as_one_of<TObject...>(functionName, make_native_args_view(args), index, expectedType);
  }

  template <class TObject>
  auto require_all_args_as(const Str &functionName, const NativeArgsView &args, const Str &expectedType)
      -> Vec<RuntimeRef<TObject>>
  {
    Vec<RuntimeRef<TObject>> values;
    values.reserve(args.size());
    for (size_t i = 0; i < args.size(); ++i)
    {
      auto value = std::dynamic_pointer_cast<TObject>(args.value_at(i));
      if (!value)
      {
        throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                               std::to_string(i + 1));
      }
      values.push_back(value);
    }
    return values;
  }

  template <class TObject>
  auto require_all_args_as(const Str &functionName, const NGArgs &args, const Str &expectedType)
      -> Vec<RuntimeRef<TObject>>
  {
    return require_all_args_as<TObject>(functionName, make_native_args_view(args), expectedType);
  }
} // namespace NG::runtime::native
