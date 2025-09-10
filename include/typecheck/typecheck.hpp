#pragma once

#include <common.hpp>
#include <ast.hpp>
#include <visitor.hpp>

#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
    using NG::ast::ASTNode;
    using NG::ast::ASTRef;

    using TypeIndex = Map<Str, CheckingRef<TypeInfo>>;

    TypeIndex type_check(ASTRef<ASTNode> ast);
}