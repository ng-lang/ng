#define RESERVED(keyword) {#keyword, TokenType::RESERVED}

// to be defined
RESERVED(and),
RESERVED(or),
// RESERVED(not), // become a builtin function instead.

// banned
// RESERVED(a),
// RESERVED(b),
// RESERVED(c),
// RESERVED(x),
// RESERVED(y),
// RESERVED(z),
// RESERVED(w),
// RESERVED(T),
// RESERVED(U),
// RESERVED(V),

// var names
RESERVED(temp),
RESERVED(it),
RESERVED(this),
//RESERVED(self),
RESERVED(test),
RESERVED(obj),
RESERVED(var),

// keywords
RESERVED(let),
RESERVED(data),
RESERVED(newtype),
RESERVED(extends),
RESERVED(implements),

// other  keywords
RESERVED(alias),
RESERVED(align),
RESERVED(alignas),
RESERVED(alignof),
RESERVED(auto),
RESERVED(cast),
RESERVED(catch),
RESERVED(compl),
RESERVED(concept),
RESERVED(const),
RESERVED(constexpr),
RESERVED(class),
RESERVED(debug),
RESERVED(decltype),
RESERVED(default),
RESERVED(define),
RESERVED(deprecated),
RESERVED(delete),
RESERVED(delegate),
RESERVED(do),
RESERVED(enum),
RESERVED(explicit),
RESERVED(extern),
RESERVED(final),
RESERVED(finally),
RESERVED(float),
RESERVED(for),
RESERVED(foreach),
RESERVED(friend),
RESERVED(from),
RESERVED(function),
RESERVED(goto),

RESERVED(immutable),
RESERVED(import),

// RESERVED(in),
RESERVED(include),
RESERVED(inline),
RESERVED(inout),
RESERVED(interface),
RESERVED(invariant),
RESERVED(is),

RESERVED(lazy),
RESERVED(macro),
RESERVED(mixin),
RESERVED(mutable),

RESERVED(namespace),
RESERVED(new),
RESERVED(nil),
RESERVED(nothrow),
RESERVED(noexcept),
RESERVED(noreturn),
RESERVED(null),
RESERVED(nullptr),

RESERVED(operator),
RESERVED(out),

RESERVED(override),
RESERVED(package),

RESERVED(pragma),
RESERVED(private),
RESERVED(protected),
RESERVED(public),

RESERVED(pure),
RESERVED(real),
RESERVED(ref),
RESERVED(register),
RESERVED(requires),
RESERVED(return),

RESERVED(scope),
RESERVED(shared),
RESERVED(static),
RESERVED(sizeof),
RESERVED(super),
RESERVED(synchornized),
RESERVED(synthetic),

RESERVED(template),
RESERVED(thread_local),
RESERVED(throw),
RESERVED(to),
RESERVED(try),
RESERVED(typedef),
RESERVED(typeid),
RESERVED(typename),
RESERVED(typeof),
RESERVED(union),

RESERVED(unittest),
RESERVED(using),

RESERVED(virtual),
RESERVED(version),
RESERVED(void),
RESERVED(volatile),

RESERVED(while),
RESERVED(with),

// other words
RESERVED(which),
RESERVED(where),
RESERVED(how),
RESERVED(why),
RESERVED(what),
RESERVED(when),

RESERVED(yield),

RESERVED(unless),
RESERVED(until),
RESERVED(repeat),

RESERVED(match),
RESERVED(treat),
RESERVED(delay),
RESERVED(force),

#undef RESERVED
