#pragma once

#include <ast.hpp>

namespace NG::module
{
    using NG::ast::ASTNode;
    using NG::ast::ASTRef;

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct ModuleLoader : NonCopyable
    {
        virtual auto load(const Vec<Str> &module) -> ASTRef<ASTNode> = 0;

        virtual ~ModuleLoader() noexcept = 0;
    };

    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct FileBasedExternalModuleLoader : public virtual ModuleLoader
    {

        Vec<Str> basePaths;

        FileBasedExternalModuleLoader(Vec<Str> basePaths) : basePaths(std::move(basePaths))
        {
        }

        auto load(const Vec<Str> &module) -> ASTRef<NG::ast::ASTNode> override;

        ~FileBasedExternalModuleLoader() override;
    };

    Str standard_library_base_path();
}
