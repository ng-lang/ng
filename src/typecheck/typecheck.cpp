
// TypeChecker implementation — split into logical sections for maintainability.
// All code is in a single translation unit to preserve anonymous namespace semantics.
//
// File structure:
//   typecheck_helpers.inl     — Helper functions + TypeChecker struct definition (~3953 lines)
//   typecheck_definitions.inl — visit(Module/TypeDef/TraitDef/ImplDef/...) (~953 lines)
//   typecheck_statements.inl  — visit(SimpleStatement/IfStatement/LoopStatement/...) (~564 lines)
//   typecheck_expressions.inl — visit(FunCallExpression/BinaryExpression/...) (~3490 lines)
//   typecheck_entry.inl       — type_check(), build_prelude_type_index() (~242 lines)

// ── Helper functions and TypeChecker struct definition ──────────────────
#include "typecheck_helpers.inl"

// ── TypeChecker visit methods: definitions ──────────────────────────────
#include "typecheck_definitions.inl"

// ── TypeChecker visit methods: statements ──────────────────────────────
#include "typecheck_statements.inl"

// ── TypeChecker visit methods: expressions ─────────────────────────────
#include "typecheck_expressions.inl"

// ── Entry points: type_check(), build_prelude_type_index() ─────────────
#include "typecheck_entry.inl"
