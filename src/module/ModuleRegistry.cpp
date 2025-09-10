#include <module.hpp>

namespace NG::module
{
    void ModuleRegistry::addModuleInfo(RuntimeRef<ModuleInfo> moduleInfo)
    {
        this->modules.insert_or_assign(moduleInfo->moduleId, moduleInfo);
    }
    auto ModuleRegistry::queryModuleById(Str moduleId) const -> RuntimeRef<ModuleInfo>
    {
        if (modules.contains(moduleId))
            return this->modules.at(moduleId);
        return {};
    }

    ModuleRegistry &get_module_registry() noexcept
    {
        static ModuleRegistry registry({}, {});
        return registry;
    }

} // namespace NG::module
