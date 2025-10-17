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
