#pragma once

#include <common.hpp>
#include <memory>

namespace NG::ast
{

    /**
     * @brief A shared pointer to an AST node.
     *
     * @tparam T The type of the AST node.
     */
    template <class T>
        requires noncopyable<T> && std::derived_from<T, ASTNode>
    using ASTRef = std::shared_ptr<T>;

    /**
     * @brief Creates a shared pointer to an AST node.
     *
     * @tparam T The type of the AST node.
     * @tparam Args The types of the arguments to the constructor of the AST node.
     * @param args The arguments to the constructor of the AST node.
     * @return A shared pointer to the new AST node.
     */
    template <class T, class... Args>
    [[nodiscard]] inline auto makeast(Args &&...args) -> ASTRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Destroys an AST node.
     *
     * @tparam T The type of the AST node.
     * @param ref The AST node to destroy.
     */
    template <class T>
    inline void destroyast(ASTRef<T> ref) noexcept
    {
    }

    /**
     * @brief Dynamically casts an AST node.
     *
     * @tparam T The type to cast to.
     * @tparam N The type to cast from.
     * @param ast The AST node to cast.
     * @return The casted AST node.
     */
    template <class T, class N>
        requires std::derived_from<T, ASTNode> && std::derived_from<N, ASTNode>
    [[nodiscard]] auto dynamic_ast_cast(ASTRef<N> ast) -> ASTRef<T>
    {
        return std::dynamic_pointer_cast<T>(ast);
    }
} // namespace NG::ast