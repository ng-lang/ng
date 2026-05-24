#include <module.hpp>
#include <runtime/value_access.hpp>

namespace NG::module
{
  namespace
  {
    auto module_tail_name(const Str &moduleId) -> Str
    {
      auto dot = moduleId.rfind('.');
      return dot == Str::npos ? moduleId : moduleId.substr(dot + 1);
    }

    auto native_descriptor_signature(const NativeModuleDescriptor &descriptor,
                                     const RuntimeRef<ModuleArtifact> &existingArtifact,
                                     const Str &name) -> NG::typecheck::CheckingRef<NG::typecheck::TypeInfo>
    {
      if (auto it = descriptor.typeIndex.find(name); it != descriptor.typeIndex.end())
      {
        return it->second;
      }
      if (auto it = descriptor.exports.types.find(name); it != descriptor.exports.types.end())
      {
        return it->second;
      }
      if (existingArtifact)
      {
        if (auto it = existingArtifact->exports.types.find(name); it != existingArtifact->exports.types.end())
        {
          return it->second;
        }
        if (auto it = existingArtifact->typeIndex.find(name); it != existingArtifact->typeIndex.end())
        {
          return it->second;
        }
      }
      return nullptr;
    }

    auto native_descriptor_has_metadata(const NativeModuleDescriptor &descriptor,
                                        const RuntimeRef<ModuleArtifact> &existingArtifact) -> bool
    {
      return !descriptor.typeIndex.empty() || !descriptor.exports.types.empty() || !descriptor.traits.empty() ||
             !descriptor.impls.empty() || (existingArtifact && (!existingArtifact->typeIndex.empty() ||
                                                                !existingArtifact->exports.types.empty() ||
                                                                !existingArtifact->traits.empty() ||
                                                                !existingArtifact->impls.empty()));
    }

    void validate_native_descriptor(const NativeModuleDescriptor &descriptor,
                                    const RuntimeRef<ModuleArtifact> &existingArtifact)
    {
      if (descriptor.moduleId.empty())
      {
        throw RuntimeException("Native module descriptor requires a module id");
      }
      if (!descriptor.requireSignatures && !native_descriptor_has_metadata(descriptor, existingArtifact))
      {
        return;
      }
      for (const auto &[name, _handler] : descriptor.functions)
      {
        auto signature = native_descriptor_signature(descriptor, existingArtifact, name);
        if (!signature)
        {
          throw RuntimeException("Native module descriptor missing NG signature for function: " +
                                 descriptor.moduleId + "::" + name);
        }
        const auto tag = signature->tag();
        if (tag != NG::typecheck::typeinfo_tag::FUNCTION && tag != NG::typecheck::typeinfo_tag::GENERIC_DEF)
        {
          throw RuntimeException("Native module descriptor signature is not callable: " +
                                 descriptor.moduleId + "::" + name);
        }
      }
    }

    auto native_artifact_exports(const NativeModuleDescriptor &descriptor,
                                 const RuntimeRef<ModuleArtifact> &existingArtifact) -> ModuleExportIndex
    {
      ModuleExportIndex exports = existingArtifact ? existingArtifact->exports : ModuleExportIndex{};
      if (!descriptor.exports.declared.empty())
      {
        exports.declared = descriptor.exports.declared;
      }
      if (exports.declared.empty() && (!descriptor.typeIndex.empty() || !descriptor.exports.types.empty()))
      {
        exports.declared.insert("*");
      }
      for (const auto &[name, type] : descriptor.exports.types)
      {
        exports.types.insert_or_assign(name, type);
      }
      if (!descriptor.typeIndex.empty())
      {
        const bool exportsAll = exports.declared.empty() || exports.declared.contains("*");
        for (const auto &[name, type] : descriptor.typeIndex)
        {
          if (exportsAll || exports.declared.contains(name))
          {
            exports.types.insert_or_assign(name, type);
          }
        }
      }
      return exports;
    }
  } // namespace

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

  void ModuleRegistry::registerNativeModuleDescriptor(RuntimeRef<NativeModuleDescriptor> descriptor)
  {
    if (!descriptor)
    {
      return;
    }
    auto existingArtifact = queryArtifactById(descriptor->moduleId);
    validate_native_descriptor(*descriptor, existingArtifact);

    auto artifact = existingArtifact ? existingArtifact : NG::runtime::makert<ModuleArtifact>();
    auto exports = native_artifact_exports(*descriptor, existingArtifact);
    auto moduleInfo = queryModuleById(descriptor->moduleId);
    auto runtimeModule = moduleInfo && moduleInfo->moduleAst && moduleInfo->runtimeModule
                             ? moduleInfo->runtimeModule
                             : NG::runtime::make_runtime_module();
    NG::runtime::bind_native_library_handlers(runtimeModule, descriptor->functions);
    for (const auto &name : exports.declared)
    {
      NG::runtime::runtime_module_add_export(runtimeModule, name);
    }
    NG::runtime::register_native_library(descriptor->moduleId, descriptor->functions);

    artifact->id = module_id_from_name(descriptor->moduleId);
    artifact->format = ModuleFormat::Native;
    artifact->originPath = descriptor->origin;
    artifact->version = descriptor->version;
    artifact->typeIndex = descriptor->typeIndex.empty() && existingArtifact ? existingArtifact->typeIndex
                                                                            : descriptor->typeIndex;
    artifact->runtimeModule = runtimeModule;
    artifact->exports = std::move(exports);
    artifact->traits = descriptor->traits.empty() && existingArtifact ? existingArtifact->traits
                                                                      : descriptor->traits;
    artifact->impls = descriptor->impls.empty() && existingArtifact ? existingArtifact->impls
                                                                    : descriptor->impls;

    if (!moduleInfo)
    {
      moduleInfo = NG::runtime::makert<ModuleInfo>(ModuleInfo{
          .moduleId = descriptor->moduleId,
          .moduleName = module_tail_name(descriptor->moduleId),
      });
    }
    moduleInfo->runtimeModule = runtimeModule;
    moduleInfo->moduleTypeIndex = artifact->typeIndex;
    moduleInfo->artifact = artifact;

    nativeDescriptors.insert_or_assign(descriptor->moduleId, descriptor);
    addModuleArtifact(artifact);
    addModuleInfo(moduleInfo);
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

  auto ModuleRegistry::queryNativeModuleDescriptor(Str moduleId) const -> RuntimeRef<NativeModuleDescriptor>
  {
    if (nativeDescriptors.contains(moduleId))
      return this->nativeDescriptors.at(moduleId);
    return {};
  }

  void ModuleRegistry::clear()
  {
    modules.clear();
    artifacts.clear();
    nativeDescriptors.clear();
  }

  ModuleRegistry &get_module_registry() noexcept
  {
    static ModuleRegistry registry({}, {});
    return registry;
  }

} // namespace NG::module
