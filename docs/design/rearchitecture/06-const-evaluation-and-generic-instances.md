# Const Evaluation and Generic Instances

> Status: vNext implementation design.
> AI-assisted document: drafted with AI assistance.
>
> Implementation status (vNext slice): the structured const-expression parser,
> `ConstInterner`/`ConstEvaluator`, const generic parameter declarations, and
> const-generic call instantiation are implemented. `const if`, per-instance
> branch selection, instance descriptors, and instance bytecode reuse remain
> pending.

## Scope

Const evaluation is a typed compiler service. It is not a second interpreter and
must not call the legacy interpreter, runtime VM, module registry, native bridge,
or mutable runtime state.

The service has two consumers:

- ordinary `const` expressions and `const if`;
- generic instance construction, including const-generic arguments.

## Semantic identities

A generic definition and an instantiated definition are different identities:

```text
GenericDefId(Result)
InstanceId(Result<i64, string>)
TypeId(Result<i64, string>)
```

Const arguments are also canonical semantic values:

```text
ConstValueId(3)
ConstValueId(1 + 2) == ConstValueId(3)
```

A const argument is never identified by its source spelling. In particular,
`array<i64, 1 + 2>` and `array<i64, 3>` must produce the same fixed-array
instance after checked evaluation.

## Initial ConstValue domain

The first evaluator is deliberately small and deterministic:

```text
unit
bool
checked signed/unsigned integers
string literals
structural tuples of const values
```

The initial type-level domain accepts integer constants only. It supports:

- integer literals;
- unary `+` and `-`;
- checked `+`, `-`, `*`, `/`, `%`;
- parentheses;
- previously evaluated const bindings once const declarations exist.

Division by zero, overflow, invalid shifts, negative fixed-array lengths, and
values outside the declared const parameter type are compile-time errors with
source spans.

Not initially permitted:

- IO, time, randomness, task creation;
- ordinary native calls;
- runtime module initialization;
- raw pointer dereference;
- uncontrolled allocation;
- mutation of runtime/session state.

## Generic const parameters

The source form is:

```ng
fun repeat<const N: i64>(value: i64) -> array<i64, N> {
    ...
}
```

A generic instance key contains both type and const arguments in declaration
order:

```text
InstanceId(repeat, [TypeArgs..., ConstValueId(N)])
```

`array<T, N>` uses the same canonical const value path. Dynamic and fixed arrays
remain distinct constructors and representations.

## Evaluation stages

1. Parse a structured const expression; do not concatenate or parse mangled
   type names.
2. Resolve names and const parameters into HIR IDs.
3. Evaluate with a typed fuel-limited `ConstEvaluator`.
4. Intern the resulting `ConstValue`.
5. Substitute type/const arguments into a canonical `InstanceId`.
6. Build or reuse the instance descriptor and lower only the selected instance.

A failed evaluation is cached as a diagnostic for the current compilation
session, not as a process-global error or runtime value.

## `const if`

For a concrete generic instance:

- the condition must evaluate to typed `bool`;
- the selected branch is fully typechecked and lowered;
- the inactive branch is syntactically valid and structurally resolved;
- syntax errors are never ignored;
- forbidden capabilities are rejected even if the branch is inactive before
  instance selection.

## Acceptance gates

The implementation is complete only when tests prove:

- equivalent const spellings share `ConstValueId` and `TypeId`;
- different const values produce distinct fixed-array/instance identities;
- evaluator overflow and capability diagnostics are exact;
- generic type and const argument substitution is deterministic;
- repeated instances reuse descriptors and bytecode identities;
- no const path reaches legacy execution or runtime module state.
