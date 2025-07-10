
#include <utility>
#include <common.hpp>

namespace NG::ast
{

    template <class T>
        requires noncopyable<T> && std::derived_from<T, ASTNode>
    using ASTRef = T *;

    template <class T, class... Args>
    inline ASTRef<T> makeast(Args &&...args)
    {
        return new T{std::move(args)...};
    }

    template <class T>
    inline void destroyast(ASTRef<T> t)
    {
        if (t != nullptr)
        {
            delete t;
        }
    }

    template <class T, class N>
    ASTRef<T> dynamic_ast_cast(ASTRef<N> ast)
    {
        return dynamic_cast<ASTRef<T>>(ast);
    }
}
