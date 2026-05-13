#pragma once

#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

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

    [[nodiscard]] auto value_at(size_t index) const -> RuntimeRef<StorageCell>
    {
      if (slotOwner && index < slotOwner->size())
      {
        return (*slotOwner)[index];
      }
      if (values && index < values->size())
      {
        return (*values)[index];
      }
      return nullptr;
    }

    [[nodiscard]] auto slot_at(size_t index) const -> RuntimeRef<StorageCell>
    {
      if (slotOwner && index < slotOwner->size())
      {
        return (*slotOwner)[index];
      }
      if (!values || index >= values->size())
      {
        return nullptr;
      }
      return (*values)[index];
    }
  };

  inline auto make_native_args_view(const NGArgs &args) -> NativeArgsView
  {
    return NativeArgsView{.values = &args};
  }

  inline auto current_native_arg_slots(const NGEnv &env) -> std::shared_ptr<Vec<RuntimeRef<StorageCell>>>
  {
    if (!env)
    {
      return nullptr;
    }
    auto state = runtime_env_get_state(env, native_arg_slots_context_key());
    return state ? std::static_pointer_cast<Vec<RuntimeRef<StorageCell>>>(state) : nullptr;
  }

  inline auto native_args_view(const NGEnv &env, const NGArgs &args) -> NativeArgsView
  {
    return NativeArgsView{
        .values = &args,
        .slotOwner = current_native_arg_slots(env),
    };
  }

  inline auto bind_native_arg_slots(const NGEnv &env, const NGArgs &args) -> std::shared_ptr<Vec<RuntimeRef<StorageCell>>>
  {
    auto slots = std::make_shared<Vec<RuntimeRef<StorageCell>>>(args);
    if (env)
    {
      runtime_env_set_state(env, native_arg_slots_context_key(), slots);
    }
    return slots;
  }

  inline auto require_arg_slot(const Str &functionName, const NativeArgsView &args, size_t index, const Str &expectedType)
      -> RuntimeRef<StorageCell>
  {
    if (args.size() < index + 1)
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    auto slot = args.slot_at(index);
    if (!slot)
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    return slot;
  }

  inline auto require_string_arg(const Str &functionName, const NativeArgsView &args, size_t index,
                                 const Str &expectedType = "a string") -> Str
  {
    auto slot = require_arg_slot(functionName, args, index, expectedType);
    if (!runtime_is_string_value(slot))
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    return runtime_string_value(slot);
  }

  inline auto require_array_arg_slot(const Str &functionName, const NativeArgsView &args, size_t index,
                                     const Str &expectedType = "an array") -> RuntimeRef<StorageCell>
  {
    auto slot = require_arg_slot(functionName, args, index, expectedType);
    if (!runtime_is_array_value(slot))
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
    return slot;
  }

  template <typename T>
  inline auto require_numeric_arg(const Str &functionName, const NativeArgsView &args, size_t index,
                                  const Str &expectedType) -> T
  {
    auto slot = require_arg_slot(functionName, args, index, expectedType);
    try
    {
      return read_numeric_cell_as<T>(slot);
    }
    catch (const std::exception &)
    {
      throw RuntimeException(functionName + "() requires " + expectedType + " at argument " +
                             std::to_string(index + 1));
    }
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

} // namespace NG::runtime::native
