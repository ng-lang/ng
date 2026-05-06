#pragma once

#include <ast.hpp>
#include <common.hpp>
#include <visitor.hpp>

#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
    using NG::ast::ASTNode;
    using NG::ast::ASTRef;

    /**
     * @brief A map from names to type information.
     */
    using TypeIndex = Map<Str, CheckingRef<TypeInfo>>;

    /**
     * @brief Type checks an AST.
     *
     * @param ast The AST to type check.
     * @param initial_index Initial type information.
     * @return A map from names to type information.
     */
    TypeIndex type_check(ASTRef<ASTNode> ast, TypeIndex initial_index = {});

    /**
     * @brief Loads and type-checks the standard library prelude module,
     *        returning a TypeIndex with all its exported symbols.
     *
     * This eliminates the need to hardcode prelude symbols in callers.
     * If the prelude cannot be found or parsed, returns an empty TypeIndex.
     */
    TypeIndex build_prelude_type_index();
} // namespace NG::typecheck
