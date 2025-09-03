#include <module.hpp>

namespace NG::module
{
    void ModuleRegistry::addModuleInfo(ModuleInfo moduleInfo)
    {
    }
    auto ModuleRegistry::queryModuleById(Str moduleId) const -> RuntimeRef<ModuleInfo>
    {
        if (moduleId.contains(moduleId))
            return this->modules.at(moduleId);
        return {};
    }

    RuntimeRef<ModuleInfo> ModuleRegistry::queryModuleByRelativeModuleRef(Str currentPath, Str currentFile, Str currentModule, Str moudleName) const
    {
        return {};
    }

    auto ModuleRegistry::queryModuleByAbsolutePath(Str modulePath, Str moduleName) const -> RuntimeRef<ModuleInfo>
    {
        return {};
    }

    RuntimeRef<ModuleInfo> ModuleRegistry::queryModuleByRelativePath(Str currentPath, Str currentFile, Str moduleName) const
    {
        return {};
    }

} // namespace NG::module
