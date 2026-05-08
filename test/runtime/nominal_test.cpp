#include "../test.hpp"
#include <intp/intp.hpp>

using namespace NG;
using namespace NG::parsing;
using namespace NG::intp;
using namespace NG::ast;

static void interpret(const Str &source)
{
  Interpreter *intp = NG::intp::stupid();

  auto ast = parse(source);

  REQUIRE(ast != nullptr);

  ast->accept(intp);

  delete intp;
  destroyast(ast);
}

TEST_CASE("interpreter: type alias transparent", "[InterpreterTest][Nominal]")
{
  interpret(R"(
        type Meters = f64;
        val x: Meters = 3.14;
        val y: f64 = x;
        assert(y == 3.14);
    )");
}

TEST_CASE("interpreter: newtype wrap", "[InterpreterTest][Nominal]")
{
  interpret(R"(
        type UserId wraps i64;
        val x = cast<UserId>(42);
        print(x);
    )");
}

TEST_CASE("interpreter: newtype is nominal", "[InterpreterTest][Nominal]")
{
  interpret(R"(
        type UserId wraps i64;
        type OrderId wraps i64;
        val uid = cast<UserId>(1);
        val oid = cast<OrderId>(2);
        print(uid);
        print(oid);
    )");
}

TEST_CASE("interpreter: multiple type aliases", "[InterpreterTest][Nominal]")
{
  interpret(R"(
        type Meters = f64;
        type Seconds = f64;
        val dist: Meters = 100.0;
        val time: Seconds = 9.58;
        val speed: f64 = dist / time;
        assert(speed > 10.0);
    )");
}

TEST_CASE("interpreter: generic object instances should behave nominally", "[InterpreterTest][Nominal][Generic]")
{
  interpret(R"(
        type Box<T> {
          property value;

          fun get() {
            return self.value;
          }
        }

        val box = new Box<i32> { value: 42 };
        assert(box.get() == 42);
        assert(box is Box<i32>);
    )");
}

TEST_CASE("interpreter: generic newtype cast should preserve nominal checks", "[InterpreterTest][Nominal][Generic]")
{
  interpret(R"(
        type Wrapper<T> wraps T;
        val x = cast<Wrapper<i32>>(42);
        assert(x is Wrapper<i32>);
    )");
}
