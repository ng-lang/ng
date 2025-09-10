#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>

namespace NG
{
    template <class T>
    using Vec = std::vector<T>;

    using Str = std::string;

    template <class T>
    using List = std::list<T>;

    template <class K, class V>
    using Map = std::unordered_map<K, V>;

    template <class T>
    using Set = std::unordered_set<T>;

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
