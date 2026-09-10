// AI-generated code; reviewed for this repository's vNext rewrite.
//
// INC-16 (D-002 A1.2 rule 8): references into aggregate storage are
// permission-gated. Struct fields cannot hold a scoped reference; enum
// payload references (the recursive-list `ref<List<T>>` pattern in
// lib/std/list.ng) are the sanctioned exception. Module consts are
// compile-time bool/i64 only, so no runtime global can hold a reference.
//
// Positive: benign generic structs, enums with ordinary payloads, and a
// recursive enum payload reference still check.
// Negative: both struct-instance construction paths reject a reference field
// (the "specialize" path used by `lib/std` instances and the source-level
// `Holder<ref<...>>` path that instantiates inline).

#include "test.hpp"

#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
  }

  void expectStructRefError(std::string_view source)
  {
    try
    {
      check(source);
      FAIL("expected a reference-field error");
    }
    catch (const typecheck::TypeError &error)
    {
      REQUIRE(std::string{error.what()}.find("references cannot be stored in struct fields") != std::string::npos);
    }
  }
} // namespace

TEST_CASE("INC-16 S3 positive: generic struct instance with value fields", "[vNext][Borrow][INC-16]")
{
  check(R"(
struct Holder<T> { value: T }
fun box(h: Holder<i64>) -> i64 { h.value }
)");
}

TEST_CASE("INC-16 S3 positive: struct fields of ordinary value types", "[vNext][Borrow][INC-16]")
{
  check(R"(
struct Pair { left: i64, right: array<i64> }
fun pair() -> Pair { Pair { left: 1, right: [2, 3] } }
)");
}

TEST_CASE("INC-16 S3 positive: reference parameters are still fine", "[vNext][Borrow][INC-16]")
{
  check(R"(
struct Box { value: i64 }
fun read(b: Box ref) -> i64 { b.value }
fun write(b: Box ref mut, v: i64) -> unit { b.value := v; }
)");
}

TEST_CASE("INC-16 S3 negative: struct field instantiated to a reference (source-level path)", "[vNext][Borrow][INC-16][Errors]")
{
  expectStructRefError(R"(
struct Holder<T> { value: T }
fun peek(h: Holder<ref<i64>>) -> i64 { 0 }
)");
}

TEST_CASE("INC-16 S3 positive: recursive enum payload references remain allowed", "[vNext][Borrow][INC-16]")
{
  check(R"(
enum List<T> {
    Cons(head: T, tail: ref<List<T>>),
    Nil,
}

fun length<T>(list: ref<List<T>>) -> i64 {
    switch (*list) {
        case Nil { return 0; }
        case Cons(_, rest) { return 1 + length(rest); }
    }
}
)");
}

TEST_CASE("INC-16 S3 positive: enum variant payload references are the sanctioned exception", "[vNext][Borrow][INC-16]")
{
  check(R"(
enum E { Empty, Payload(r: i64 ref) }
fun use(e: E) -> i64 {
  switch (e) {
    case Empty { return 0; }
    case Payload(r) { return *r; }
  }
}
)");
}