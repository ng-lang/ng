#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{
  auto module_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Module",
        .layout = TypeLayout{.name = "Module", .kind = LayoutKind::DYNAMIC},
        .showCellHandler = [](const RuntimeRef<StorageCell> &) { return Str{"[Module]"}; },
        .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; },
        .respondCellHandler =
            [](const RuntimeRef<StorageCell> &self, const Str &member, const NGEnv &env,
               const NGArgs &args) -> RuntimeRef<StorageCell> {
              if (!runtime_is_module_value(self))
              {
                return nullptr;
              }
              auto functions = runtime_module_functions(self);
              if (functions.contains(member))
              {
                auto result = functions[member](self, env, args);
                return result ? result : unit_cell();
              }
              if (auto slot = runtime_module_slot_named(self, member))
              {
                return slot;
              }
              return nullptr;
            },
    });
    return type;
  }
} // namespace NG::runtime
