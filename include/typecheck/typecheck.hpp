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
     * @return A map from names to type information.
     */
    TypeIndex type_check(ASTRef<ASTNode> ast);
} // namespace NG::typecheck