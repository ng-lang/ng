#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <module.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  void register_context_for_gc(NGContext *context);
  void unregister_context_for_gc(NGContext *context);

  const static NGCallable DUMMY = [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<NGObject> {
    return makert<NGUnit>();
  };

  auto make_runtime_env(const RuntimeRef<NGContext> &context) -> NGEnv
  {
    auto env = makert<RuntimeEnv>();
    if (context)
    {
      env->symbols = context->symbol_table();
      env->runtimeState = context->runtimeState;
      env->executionContext = context;
    }
    return env;
  }

  auto fork_runtime_env(const NGEnv &env) -> NGEnv
  {
    auto forked = makert<RuntimeEnv>();
    if (!env)
    {
      return forked;
    }
    forked->symbols = env->symbols;
    forked->runtimeState = env->runtimeState;
    forked->selfSlot = env->selfSlot;
    forked->executionContext = env->executionContext;
    return forked;
  }

  auto runtime_env_with_self(const NGEnv &env, const NGSelf &self) -> NGEnv
  {
    auto next = fork_runtime_env(env);
    next->selfSlot = make_boxed_storage_cell(self ? self : makert<NGUnit>(), StorageClass::TEMPORARY);
    next->selfSlot->name = "self";

    auto baseContext = next->executionContext;
    auto dispatchContext = baseContext ? baseContext->fork() : makert<NGContext>();
    if (next->symbols)
    {
      dispatchContext->adopt_symbol_table(next->symbols);
    }
    dispatchContext->define("self", self ? self : makert<NGUnit>());
    next->executionContext = dispatchContext;
    return next;
  }

  auto runtime_env_context(const NGEnv &env) -> RuntimeRef<NGContext>
  {
    if (!env)
    {
      return nullptr;
    }
    if (!env->executionContext)
    {
      env->executionContext = makert<NGContext>();
      if (env->symbols)
      {
        env->executionContext->adopt_symbol_table(env->symbols);
      }
      if (env->selfSlot)
      {
        env->executionContext->define("self", env->selfSlot->boxedValue ? env->selfSlot->boxedValue : makert<NGUnit>());
      }
    }
    return env->executionContext;
  }

  void runtime_env_set_state(const NGEnv &env, Str name, std::shared_ptr<void> value)
  {
    if (!env)
    {
      return;
    }
    env->runtimeState.insert_or_assign(std::move(name), std::move(value));
  }

  auto runtime_env_get_state(const NGEnv &env, const Str &name) -> std::shared_ptr<void>
  {
    if (!env || !env->runtimeState.contains(name))
    {
      return nullptr;
    }
    return env->runtimeState.at(name);
  }

  NGContext::NGContext()
  {
    register_context_for_gc(this);
  }

  NGContext::~NGContext()
  {
    unregister_context_for_gc(this);
  }

  auto NGContext::fork() -> RuntimeRef<NGContext>
  {
    auto ctx = makert<NGContext>();
    ctx->parent = this;
    ctx->symbols = symbol_table();

    return ctx;
  }

  auto NGContext::get(Str name) -> RuntimeRef<NGObject>
  {
    if (auto slot = get_slot(name))
    {
      return slot->boxedValue;
    }
    return nullptr;
  }

  auto NGContext::get_slot(Str name) -> RuntimeRef<StorageCell>
  {
    if (objectSlots.contains(name))
    {
      return objectSlots.at(name);
    }
    if (parent != nullptr)
    {
      return parent->get_slot(name);
    }
    if (auto globals = symbol_table(); globals && globals->objectSlots.contains(name))
    {
      return globals->objectSlots.at(name);
    }
    return nullptr;
  }

  void NGContext::set(Str name, RuntimeRef<NGObject> value)
  {
    if (objectSlots.contains(name))
    {
      runtime_sync_storage_cell(objectSlots[name], value);
    }
    else if (parent != nullptr)
    {
      parent->set(name, value);
    }
    else if (auto globals = symbol_table(); globals && globals->objectSlots.contains(name))
    {
      runtime_sync_storage_cell(globals->objectSlots[name], value);
    }
    else
    {
      throw RuntimeException("Invalid assignment to " + name);
    }
  }

  void NGContext::define(Str name, RuntimeRef<NGObject> value)
  {
    if (locals.contains(name))
    {
      // todo: redefine, consider as error?
      throw RuntimeException("Redefine " + name);
    }
    locals.insert(name);
    auto slot = make_boxed_storage_cell(value, parent == nullptr ? StorageClass::GLOBAL : StorageClass::TEMPORARY);
    slot->name = name;
    slot->ownerContext = this;
    if (parent == nullptr)
    {
      symbol_table()->objectSlots[name] = slot;
    }
    else
    {
      objectSlots[name] = slot;
    }
  }

  void NGContext::define_function(Str name, NGCallable value)
  {
    if (symbol_table()->functions.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbol_table()->functions[name] = value;
  }

  void NGContext::define_type(Str name, RuntimeRef<NGType> type)
  {
    if (symbol_table()->types.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbol_table()->types[name] = type;
  }

  void NGContext::define_variant_type(Str name, RuntimeRef<NGType> type)
  {
    symbol_table()->variantTypes[name] = type;
  }

  void NGContext::define_module(Str name, RuntimeRef<NGModule> module)
  {
    if (symbol_table()->modules.contains(name))
    {
      throw RuntimeException("Redefine " + name);
    }
    symbol_table()->modules[name] = module;
  }

  auto NGContext::has_object(Str name, bool global) -> bool
  {
    return get_slot(name) != nullptr;
  }
  auto NGContext::has_function(Str name, bool global) -> bool
  {
    return symbol_table() && symbol_table()->functions.contains(name);
  }
  auto NGContext::has_module(Str name, bool global) -> bool
  {
    return symbol_table() && symbol_table()->modules.contains(name);
  }
  auto NGContext::has_type(Str name, bool global) -> bool
  {
    return symbol_table() && symbol_table()->types.contains(name);
  }

  auto NGContext::get_function(Str name) -> NGCallable
  {
    if (auto globals = symbol_table(); globals && globals->functions.contains(name))
    {
      return globals->functions.at(name);
    }
    return DUMMY;
  }
  auto NGContext::get_module(Str name) -> RuntimeRef<NGModule>
  {
    if (auto globals = symbol_table(); globals && globals->modules.contains(name))
    {
      return globals->modules.at(name);
    }
    return nullptr;
  }

  auto NGContext::get_type(Str name) -> RuntimeRef<NGType>
  {
    if (auto globals = symbol_table(); globals && globals->types.contains(name))
    {
      return globals->types.at(name);
    }
    return nullptr;
  }

  auto NGContext::get_variant_type(Str name) -> RuntimeRef<NGType>
  {
    if (auto globals = symbol_table(); globals && globals->variantTypes.contains(name))
    {
      return globals->variantTypes.at(name);
    }
    return nullptr;
  }

  auto NGContext::symbol_table() -> RuntimeRef<RuntimeSymbolTable>
  {
    if (!symbols)
    {
      symbols = makert<RuntimeSymbolTable>();
    }
    return symbols;
  }

  void NGContext::set_runtime_state(Str name, std::shared_ptr<void> value)
  {
    runtimeState.insert_or_assign(std::move(name), std::move(value));
  }

  auto NGContext::get_runtime_state(const Str &name) const -> std::shared_ptr<void>
  {
    if (runtimeState.contains(name))
    {
      return runtimeState.at(name);
    }
    if (parent != nullptr)
    {
      return parent->get_runtime_state(name);
    }
    return nullptr;
  }

  void NGContext::clear_runtime_state(const Str &name)
  {
    runtimeState.erase(name);
  }

  void NGContext::summary()
  {
    auto context = this;
    debug_log("Context objects size", context->objectSlots.size());

    for (const auto &[name, slot] : context->objectSlots)
    {
      debug_log("Context object", "key:", name, "value:", runtime_value_show(slot ? slot->boxedValue : nullptr));
    }

    auto globals = context->symbol_table();
    debug_log("Context globals size", globals->objectSlots.size());
    for (const auto &[name, slot] : globals->objectSlots)
    {
      debug_log("Global object", "key:", name, "value:", runtime_value_show(slot ? slot->boxedValue : nullptr));
    }

    debug_log("Context modules size", globals->modules.size());

    for (const auto &pair : globals->modules)
    {
      debug_log("Context module", "name:", pair.first, "value:", code(pair.second->size()));
    }

    for (const auto &type : globals->types)
    {
      debug_log("Context types", "name:", type.first,
                "members:", type.second->properties.size() + type.second->memberFunctions.size());
    }
  }
} // namespace NG::runtime
