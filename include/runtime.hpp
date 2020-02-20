
#ifndef __NG_RUNTIME_HPP
#define __NG_RUNTIME_HPP

#include <fwd.hpp>
#include "common.hpp"

namespace NG::runtime {


    struct NGInvocationContext {
        Vec<NGObject *> params;
    };

    struct NGObject {
        enum class tag_t : uintptr_t {
            NG_NIL,
            NG_NUM,
            NG_BOOL,
            NG_STR,
            NG_COMPOSITE,
            NG_ARRAY,
            NG_CUSTOMIZED,
        };

        tag_t tag;

        union {
            double number;
            bool boolean;
            const Str *str;
            NGArray *array;
        } value;

        NGObject() = default;

        Str show();

        static NGObject *number(double number);

        static NGObject *boolean(bool boolean);

        static NGObject *str(const Str *str);

        static NGObject *array(NGArray *array);

        double numValue() {
            if (tag == tag_t::NG_NUM) {
                return value.number;
            }
            throw IllegalTypeException("Not a number");
        }

        bool boolValue() {
            if (tag == tag_t::NG_BOOL) {
                return value.boolean;
            }
            throw IllegalTypeException("Not a boolean");
        }

        const Str *strValue() {
            if (tag == tag_t::NG_STR) {
                return value.str;
            }
            throw IllegalTypeException("Not a string");
        }

        bool equals(NGObject *ngObject);

    };


    struct NGDefinition {
        Str name;
        NG::AST::ASTNode *defbody;
    };

    struct NGModule {
        Map<Str, NGDefinition *> defs;
    };

    struct NGArray {
        NGObject::tag_t itemTag;
        Vec<NGObject *> items;

        bool equals(NGArray *array);
    };

    template<class T>
    using NGRef = T *;

    struct NGContext {
        Map<Str, NGObject *> objects;
        Map<Str, std::function<void(NGContext &ctx)>> handlers;
        Map<Str, std::function<void(NGContext &ctx, NGInvocationContext &invCtx)>> functions;

        Map<Str, NGModule *> modules;

        NGObject *retVal;

        ~NGContext();
    };

}

#endif // __NG_RUNTIME_HPP
