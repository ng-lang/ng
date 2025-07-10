#pragma once

#include <memory>
#include <common.hpp>

namespace NG::ast
{

    template <class T>
        requires noncopyable<T> && std::derived_from<T, ASTNode>
    using ASTRef = std::shared_ptr<T>;

    template <class T, class... Args>
    inline auto makeast(Args &&...args) -> ASTRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    template <class T>
    inline void destroyast(ASTRef<T> ref)
    {
    }

    template <class T, class N>
    auto dynamic_ast_cast(ASTRef<N> ast) -> ASTRef<T>
    {
        return std::dynamic_pointer_cast<T>(ast);
    }
}