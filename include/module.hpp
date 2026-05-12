#pragma once

#include <ast.hpp>
#include <intp/runtime.hpp>
#include <typecheck/typecheck.hpp>

namespace NG::orgasm
{
    struct BytecodeModule;
    class VM;
}

namespace NG::library::prelude
{
    /**
     * @brief Registers the prelude library.
     */
    void do_register();

    /**
     * @brief Registers native prelude functions with the ORGASM VM.
     */
    void register_vm_natives(NG::orgasm::VM &vm);

    /**
     * @brief Returns the names of all native prelude functions.
     */
    Vec<Str> native_function_names();
} // namespace NG::library::prelude
namespace NG::library::imgui
{
    /**
     * @brief Registers the imgui library.
     */
    void do_register();

    /**
     * @brief Registers native imgui functions with the ORGASM VM.
     */
    void register_vm_natives(NG::orgasm::VM &vm);

    /**
     * @brief Returns the names of all native imgui functions.
     */
    Vec<Str> native_function_names();
} // namespace NG::library::imgui

namespace NG::module
{
    using NG::ast::ASTNode;
    using NG::ast::ASTRef;
    using NG::runtime::RuntimeRef;
    using NG::typecheck::TypeIndex;

    /**
     * @brief Contains information about a module.
     */
    struct ModuleInfo
    {
        Str moduleId;                                    ///< The unique ID of the module.
        Str moduleName;                                  ///< The name of the module.
        Str moduleSource;                                ///< The source code of the module.
        ASTRef<NG::ast::ASTNode> moduleAst;              ///< The AST of the module.
        Str moduleAbsolutePath;                          ///< The absolute path to the module.
        Str moduleLoadingLocation;                       ///< The location from which the module was loaded.
        RuntimeRef<NG::runtime::StorageCell> runtimeModule; ///< The runtime representation of the module.
        TypeIndex moduleTypeIndex{};                     ///< The type index of the module.
        std::shared_ptr<NG::orgasm::BytecodeModule> bytecodeModule; ///< The bytecode representation.
    };

    /**
     * @brief A registry for modules.
     */
    class ModuleRegistry : NonCopyable
    {
        Map<Str, RuntimeRef<ModuleInfo>> modules; ///< The modules in the registry.
        Vec<Str> basePaths;                       ///< The base paths for module resolution.

      public:
        ModuleRegistry(Map<Str, RuntimeRef<ModuleInfo>> modules, Vec<Str> basePaths)
            : modules(modules), basePaths(basePaths)
        {
        }

        /**
         * @brief Adds a module to the registry.
         *
         * @param moduleInfo The module to add.
         */
        void addModuleInfo(RuntimeRef<ModuleInfo> moduleInfo);
        /**
         * @brief Queries a module by its ID.
         *
         * @param moduleId The ID of the module.
         * @return The module info.
         */
        RuntimeRef<ModuleInfo> queryModuleById(Str moduleId) const;

        /**
         * @brief Clears all registered modules.
         */
        void clear();
    };

    /**
     * @brief An interface for module loaders.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct ModuleLoader : NonCopyable
    {
        /**
         * @brief Loads a module.
         *
         * @param module The path to the module.
         * @return The loaded module.
         */
        virtual auto load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo> = 0;

        virtual ~ModuleLoader() noexcept = 0;
    };

    /**
     * @brief A module loader for file-based external modules.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct FileBasedExternalModuleLoader : public virtual ModuleLoader
    {

        Vec<Str> basePaths; ///< The base paths for module resolution.

        FileBasedExternalModuleLoader(Vec<Str> basePaths) : basePaths(std::move(basePaths)) {}

        auto load(const Vec<Str> &module) -> RuntimeRef<ModuleInfo> override;

        ~FileBasedExternalModuleLoader() override;
    };

    /**
     * @brief Returns the base path of the standard library.
     *
     * @return The base path of the standard library.
     */
    Str standard_library_base_path();
    /**
     * @brief Returns the global module registry.
     *
     * @return The global module registry.
     */
    ModuleRegistry &get_module_registry() noexcept;

    /**
     * @brief Clears the file-based module loader cache.
     */
    void clear_module_loader_cache() noexcept;
} // namespace NG::module
