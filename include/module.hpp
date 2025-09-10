#pragma once

#include <ast.hpp>
#include <intp/runtime.hpp>

namespace NG::library::prelude
{
    void do_register();
}
namespace NG::library::imgui
{
    void do_register();
}

namespace NG::module
{
    using NG::ast::ASTNode;
    using NG::ast::ASTRef;
    using NG::runtime::RuntimeRef;

    struct ModuleInfo
    {
        Str moduleId;
        Str moduleAbsolutePath;
        Str moduleName;
        Str moduleLoadingLocation;
        ASTRef<NG::ast::ASTNode> moduleAst;
        Str moduleSource;
        RuntimeRef<NG::runtime::NGModule> runtimeModule;
    };

    class ModuleRegistry : NonCopyable
    {
        Map<Str, RuntimeRef<ModuleInfo>> modules;
        Vec<Str> basePaths;

    public:
        ModuleRegistry(Map<Str, RuntimeRef<ModuleInfo>> modules, Vec<Str> basePaths)
            : modules(modules), basePaths(basePaths) {}

        void addModuleInfo(RuntimeRef<ModuleInfo> moduleInfo);
        RuntimeRef<ModuleInfo> queryModuleById(Str moduleId) const;
    };

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct ModuleLoader : NonCopyable
    {
        virtual auto load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo> = 0;

        virtual ~ModuleLoader() noexcept = 0;
    };

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct FileBasedExternalModuleLoader : public virtual ModuleLoader
    {

        Vec<Str> basePaths;

        FileBasedExternalModuleLoader(Vec<Str> basePaths) : basePaths(std::move(basePaths))
        {
        }

        auto load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo> override;

        ~FileBasedExternalModuleLoader() override;
    };

    Str standard_library_base_path();
    ModuleRegistry &get_module_registry() noexcept;
}
