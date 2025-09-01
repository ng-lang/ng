#pragma once

#include <ast.hpp>
#include <intp/runtime.hpp>

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
}
