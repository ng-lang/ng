
#include <memory>

namespace NG::AST {

    template<class T>
    using ASTRef = std::shared_ptr<T>;

    template<class T, class... Args>
    inline ASTRef<T> makeast(Args &&... args) {
        return std::make_shared<T>(std::move(args)...);
    }

    template<class T>
    inline void destroyast(ASTRef<T> t) {
    }

    template<class T, class N>
    ASTRef<T> dynamic_ast_cast(ASTRef<N> ast) {
        return std::dynamic_pointer_cast<T>(ast);
    }
}