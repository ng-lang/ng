#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <list>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Extracts the keys from an unordered_map.
 *
 * @tparam K The key type of the map.
 * @tparam V The value type of the map.
 * @param map The map from which to extract keys.
 * @return A vector containing the keys of the map.
 */
template <class K, class V>
auto keys_of(const std::unordered_map<K, V> &map) -> std::vector<K>
{
    std::vector<K> keys{};
    keys.reserve(map.size());
    std::transform(map.begin(), map.end(), std::back_inserter(keys), [](const auto &pair) { return pair.first; });
    return keys;
}

/**
 * @brief Concept for container-like types.
 *
 * A type is considered a container if it has `begin()`, `end()`, and `size()` methods,
 * and is not a `std::string`.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept Container = !std::same_as<std::decay_t<T>, std::string> && requires(T t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for non-container types.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept NonContainer = !Container<T>;

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
