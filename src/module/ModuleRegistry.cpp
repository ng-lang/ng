#include <module.hpp>

namespace NG::module
{
  void ModuleRegistry::addModuleInfo(RuntimeRef<ModuleInfo> moduleInfo)
  {
    this->modules.insert_or_assign(moduleInfo->moduleId, moduleInfo);
    if (moduleInfo->artifact)
    {
      addModuleArtifact(moduleInfo->artifact);
    }
  }

  void ModuleRegistry::addModuleArtifact(RuntimeRef<ModuleArtifact> artifact)
  {
    if (!artifact)
    {
      return;
    }
    this->artifacts.insert_or_assign(artifact->id.canonicalName, artifact);
  }

  auto ModuleRegistry::queryModuleById(Str moduleId) const -> RuntimeRef<ModuleInfo>
  {
    if (modules.contains(moduleId))
      return this->modules.at(moduleId);
    return {};
  }

  auto ModuleRegistry::queryArtifactById(Str moduleId) const -> RuntimeRef<ModuleArtifact>
  {
    if (artifacts.contains(moduleId))
      return this->artifacts.at(moduleId);
    return {};
  }

  void ModuleRegistry::clear()
  {
    modules.clear();
    artifacts.clear();
  }

  ModuleRegistry &get_module_registry() noexcept
  {
    static ModuleRegistry registry({}, {});
    return registry;
  }

} // namespace NG::module
