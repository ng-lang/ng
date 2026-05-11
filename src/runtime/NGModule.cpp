
#include <debug.hpp>
#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>
namespace NG::runtime
{

  NGModule::NGModule(RuntimeRef<NGContext> ctx)
  {
    auto symbols = ctx->symbol_table();
    for (const auto &[name, slot] : symbols->objectSlots)
    {
      objects.insert_or_assign(name, slot ? slot->boxedValue : nullptr);
    }
    functions = symbols->functions;
    types = symbols->types;
    exports.insert(symbols->exports.begin(), symbols->exports.end());
    imports.insert(symbols->imported.begin(), symbols->imported.end());
  }

  void NGModule::set_native_state(Str name, std::shared_ptr<void> value)
  {
    native_state.insert_or_assign(std::move(name), std::move(value));
  }

  auto NGModule::get_native_state(const Str &name) const -> std::shared_ptr<void>
  {
    auto it = native_state.find(name);
    if (it == native_state.end())
    {
      return nullptr;
    }
    return it->second;
  }

  void NGModule::clear_native_state(const Str &name)
  {
    native_state.erase(name);
  }

  auto NGModule::moduleType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Module",
        .layout = TypeLayout{.name = "Module", .kind = LayoutKind::DYNAMIC},
        .showHandler = [](const NGSelf &) { return Str{"[Module]"}; },
        .boolHandler = [](const NGSelf &) { return true; },
        .respondHandler =
            [](const NGSelf &self, const Str &member, const NGCtx &context, const NGArgs &args) -> RuntimeRef<NGObject> {
              auto module = std::dynamic_pointer_cast<NGModule>(self);
              if (!module)
              {
                return nullptr;
              }
              if (module->functions.contains(member))
              {
                auto newContext = context ? context->fork() : makert<NGContext>();
                newContext->define("self", self);
                auto result = module->functions[member](self, newContext, args);
                return result ? result : makert<NGUnit>();
              }
              if (module->objects.contains(member))
              {
                return module->objects[member];
              }
              return nullptr;
            },
    });
    return type;
  }

  NGModule::~NGModule() = default;
} // namespace NG::runtime
