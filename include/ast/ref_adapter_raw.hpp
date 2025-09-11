#pragma once

#include <utility>
#include <common.hpp>

namespace NG::ast
{

    /**
     * @brief A raw pointer to an AST node.
     *
     * @tparam T The type of the AST node.
     */
    template <class T>
        requires noncopyable<T> && std::derived_from<T, ASTNode>
    using ASTRef = T *;

    /**
     * @brief Creates a raw pointer to an AST node.
     *
     * @tparam T The type of the AST node.
     * @tparam Args The types of the arguments to the constructor of the AST node.
     * @param args The arguments to the constructor of the AST node.
     * @return A raw pointer to the new AST node.
     */
    template <class T, class... Args>
    inline auto makeast(Args &&...args) -> ASTRef<T>
    {
        return new T{std::forward<Args>(args)...}; // NOLINT(cppcoreguidelines-owning-memory)
    }

    /**
     * @brief Destroys an AST node.
     *
     * @tparam T The type of the AST node.
     * @param ref The AST node to destroy.
     */
    template <class T>
    inline void destroyast(ASTRef<T> ref)
    {
        delete ref; // NOLINT(cppcoreguidelines-owning-memory)
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
    auto dynamic_ast_cast(ASTRef<N> ast) -> ASTRef<T>
    {
        return dynamic_cast<ASTRef<T>>(ast);
    }
}
