#pragma once

#include <vector>
#include <string>
#include <unordered_map>

namespace NG
{
    template <class T>
    using Vec = std::vector<T>;

    using Str = std::string;

    template <class K, class V>
    using Map = std::unordered_map<K, V>;

    struct Token;

} // namespace NG

namespace NG::ast
{
    struct ASTNode;

    struct AstVisitor;

    struct Definition;
    struct Expression;
    struct Statement;

} // namespace NG::AST

namespace NG::runtime
{
    struct NGObject;
    struct NGModule;
    struct NGContext;
    struct NGType;
    struct NGArray;
} // namespace NG::runtime
