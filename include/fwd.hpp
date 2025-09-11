#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <list>

namespace NG
{
    /**
     * @brief Alias for std::vector.
     *
     * @tparam T The type of elements in the vector.
     */
    template <class T>
    using Vec = std::vector<T>;

    /**
     * @brief Alias for std::string.
     */
    using Str = std::string;

    /**
     * @brief Alias for std::list.
     *
     * @tparam T The type of elements in the list.
     */
    template <class T>
    using List = std::list<T>;

    /**
     * @brief Alias for std::unordered_map.
     *
     * @tparam K The type of the keys.
     * @tparam V The type of the values.
     */
    template <class K, class V>
    using Map = std::unordered_map<K, V>;

    /**
     * @brief Alias for std::unordered_set.
     *
     * @tparam T The type of the elements.
     */
    template <class T>
    using Set = std::unordered_set<T>;

    // Forward declaration for Token.
    struct Token;

} // namespace NG

namespace NG::ast
{
    // Forward declaration for ASTNode.
    struct ASTNode;

    // Forward declaration for AstVisitor.
    struct AstVisitor;

    // Forward declaration for Definition.
    struct Definition;

    // Forward declaration for Expression.
    struct Expression;

    // Forward declaration for Statement.
    struct Statement;

} // namespace NG::ast

namespace NG::runtime
{
    // Forward declaration for NGObject.
    struct NGObject;

    // Forward declaration for NGModule.
    struct NGModule;

    // Forward declaration for NGContext.
    struct NGContext;

    // Forward declaration for NGType.
    struct NGType;

    // Forward declaration for NGArray.
    struct NGArray;
} // namespace NG::runtime
