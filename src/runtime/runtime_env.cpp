#include <intp/runtime.hpp>

namespace NG::runtime
{
  auto make_runtime_env(const NGSymbols &symbols) -> NGEnv
  {
    auto env = makert<RuntimeEnv>();
    env->symbols = symbols;
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
    return forked;
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
} // namespace NG::runtime
