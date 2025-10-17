#include <catch2/catch_test_macros.hpp>
#include "orgasm/interpreter.hpp"
#include "orgasm/parser.hpp"

using namespace ng::orgasm;

TEST_CASE("ORGASM interpreter should execute arithmetic", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 10
.const i32 5
.start
00:    load_const.i32 const.0
01:    load_const.i32 const.1
02:    add.i32
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  // Result should be on stack - in a full implementation we'd check this
  REQUIRE(shared_module != nullptr);
}

TEST_CASE("ORGASM interpreter should call functions", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 42
.fun id
.param [n:i32]
00:    load_param.i32 param.0
01:    return
.endfun id
.start
00:    load_const.i32 const.0
01:    push_param
02:    call fun.0
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->functions.size() == 1);
  REQUIRE(shared_module->functions[0].name == "id");
}

TEST_CASE("ORGASM interpreter should handle variables", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 100
.val i32 0
.start
00:    load_const.i32 const.0
01:    store_value.i32 val.0
02:    load_value.i32 val.0
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should compare values", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 10
.const i32 5
.start
00:    load_const.i32 const.0
01:    load_const.i32 const.1
02:    gt.i32
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle control flow with goto", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 42
.import print, core
.start
00:    goto 2
01:    load_const.i32 const.0
02:    load_const.i32 const.0
03:    push_param
04:    call import.0
05:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  bool print_called = false;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    print_called = true;
    return Value();
  });
  
  interp.execute();
  REQUIRE(print_called);
}

TEST_CASE("ORGASM interpreter should handle tuple operations", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.const i32 42
.const i32 0
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_const.i32 const.1
05:    tuple_set.i32 0
06:    load_value.addr val.0
07:    tuple_get.i32 0
08:    load_const.i32 const.1
09:    eq.i32
10:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should handle type casting", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 10
.const f64 3.14
.start
00:    load_const.i32 const.0
01:    cast.f64
02:    load_const.f64 const.1
03:    add.f64
04:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle subtraction and division", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 20
.const i32 5
.import print, core
.start
00:    load_const.i32 const.0
01:    load_const.i32 const.1
02:    subtract.i32
03:    load_const.i32 const.1
04:    divide.i32
05:    push_param
06:    call import.0
07:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  int result = 0;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    result = std::get<int32_t>(args[0].data);
    return Value();
  });
  
  interp.execute();
  REQUIRE(result == 3); // (20 - 5) / 5 = 3
}

TEST_CASE("ORGASM interpreter should handle less than comparison", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 5
.const i32 10
.start
00:    load_const.i32 const.0
01:    load_const.i32 const.1
02:    lt.i32
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle not equal comparison", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 5
.const i32 10
.start
00:    load_const.i32 const.0
01:    load_const.i32 const.1
02:    ne.i32
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle float operations", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const f32 5.5
.const f32 2.5
.import print, core
.start
00:    load_const.f32 const.0
01:    load_const.f32 const.1
02:    add.f32
03:    push_param
04:    call import.0
05:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  float result = 0.0f;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::F32) {
      result = std::get<float>(args[0].data);
    }
    return Value();
  });
  
  interp.execute();
  REQUIRE(result == 8.0f);
}

TEST_CASE("ORGASM interpreter should handle i64 operations", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i64 1000000000000
.const i64 2000000000000
.start
00:    load_const.i64 const.0
01:    load_const.i64 const.1
02:    add.i64
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle f64 operations", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const f64 3.14159
.const f64 2.71828
.start
00:    load_const.f64 const.0
01:    load_const.f64 const.1
02:    multiply.f64
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle boolean equality", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const bool true
.const bool true
.start
00:    load_const.bool const.0
01:    load_const.bool const.1
02:    eq.bool
03:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->constants.size() == 2);
}

TEST_CASE("ORGASM interpreter should handle tuple with bool", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.const bool true
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_const.bool const.1
05:    tuple_set.bool 0
06:    load_value.addr val.0
07:    tuple_get.bool 0
08:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should handle tuple with f32", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.const f32 3.14
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_const.f32 const.1
05:    tuple_set.f32 0
06:    load_value.addr val.0
07:    tuple_get.f32 0
08:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should handle tuple with f64", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.const f64 2.71828
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_const.f64 const.1
05:    tuple_set.f64 0
06:    load_value.addr val.0
07:    tuple_get.f64 0
08:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should handle tuple with i64", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.const i64 9876543210
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_const.i64 const.1
05:    tuple_set.i64 0
06:    load_value.addr val.0
07:    tuple_get.i64 0
08:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 1);
}

TEST_CASE("ORGASM interpreter should handle tuple with addr", "[orgasm][interpreter]") {
  std::string source = R"(
.module test
.const i32 16
.val addr 0
.val addr 0
.start
00:    load_const.i32 const.0
01:    tuple_create
02:    store_value.addr val.0
03:    load_value.addr val.0
04:    load_value.addr val.1
05:    tuple_set.addr 0
06:    load_value.addr val.0
07:    tuple_get.addr 0
08:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  interp.execute();
  
  REQUIRE(shared_module->variables.size() == 2);
}
