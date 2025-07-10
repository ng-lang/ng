#pragma once

#include <utility>
#include <common.hpp>

namespace NG::ast
{

    template <class T>
        requires noncopyable<T> && std::derived_from<T, ASTNode>
    using ASTRef = T *;

    template <class T, class... Args>
    inline auto makeast(Args &&...args) -> ASTRef<T>
    {
        return new T{std::forward<Args>(args)...}; // NOLINT(cppcoreguidelines-owning-memory)
    }

    template <class T>
    inline void destroyast(ASTRef<T> ref)
    {
        delete ref; // NOLINT(cppcoreguidelines-owning-memory)
    }

    template <class T, class N>
    auto dynamic_ast_cast(ASTRef<N> ast) -> ASTRef<T>
    {
        return dynamic_cast<ASTRef<T>>(ast);
    }
}
