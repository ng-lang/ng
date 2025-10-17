#include <catch2/catch_test_macros.hpp>
#include "orgasm/interpreter.hpp"
#include "orgasm/parser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace ng::orgasm;

TEST_CASE("ORGASM integration with 01.id.l2.asm", "[orgasm][integration]") {
  std::string source = R"(
// Level-2 ORGASM for 01.id.ng

.module default
.symbols [id, n, print]
.import print, core
.const i32 1
.start
00:    load_const.i32 const.0    // 1
01:    push_param
02:    call fun.0                // id
03:    push_param
04:    call import.0             // print
05:    return
.fun id
.param [n:i32]
00:    load_param.i32 param.0    // n
01:    return
.endfun id
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  
  REQUIRE(module != nullptr);
  REQUIRE(module->name == "default");
  REQUIRE(module->symbols.size() == 3);
  REQUIRE(module->imports.size() == 1);
  REQUIRE(module->constants.size() == 1);
  REQUIRE(module->functions.size() == 1);
  REQUIRE(module->start_block != nullptr);
  
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  Interpreter interp(shared_module);
  
  // Register a mock print function
  bool print_called = false;
  int print_value = 0;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    print_called = true;
    if (!args.empty() && args[0].type == PrimitiveType::I32) {
      print_value = std::get<int32_t>(args[0].data);
    }
    return Value(); // Unit value
  });
  
  // Execute the module
  interp.execute();
  
  REQUIRE(print_called);
  REQUIRE(print_value == 1);
}

TEST_CASE("ORGASM integration with arithmetic operations", "[orgasm][integration]") {
  std::string source = R"(
.module test
.const i32 5
.const i32 3
.const i32 2
.import print, core
.val i32 0
.start
00:    load_const.i32 const.0    // 5
01:    load_const.i32 const.1    // 3
02:    add.i32                   // 5 + 3 = 8
03:    load_const.i32 const.2    // 2
04:    multiply.i32              // 8 * 2 = 16
05:    store_value.i32 val.0     // result = 16
06:    load_value.i32 val.0
07:    push_param
08:    call import.0             // print(16)
09:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  bool print_called = false;
  int print_value = 0;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    print_called = true;
    if (!args.empty() && args[0].type == PrimitiveType::I32) {
      print_value = std::get<int32_t>(args[0].data);
    }
    return Value();
  });
  
  interp.execute();
  
  REQUIRE(print_called);
  REQUIRE(print_value == 16);
}

TEST_CASE("ORGASM integration with goto control flow", "[orgasm][integration]") {
  std::string source = R"(
.module test
.const i32 100
.import print, core
.start
00:    goto 3
01:    load_const.i32 const.0
02:    goto 4
03:    load_const.i32 const.0
04:    push_param
05:    call import.0
06:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  int result = 0;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::I32) {
      result = std::get<int32_t>(args[0].data);
    }
    return Value();
  });
  
  interp.execute();
  
  REQUIRE(result == 100);
}

TEST_CASE("ORGASM integration with string operations", "[orgasm][integration]") {
  std::string source = R"(
.module test
.str [hello]
.str [world]
.import print, core
.start
00:    load_str str.0
01:    push_param
02:    call import.0
03:    load_str str.1
04:    push_param
05:    call import.0
06:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  std::vector<std::string> printed_strings;
  interp.register_import("print", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::ADDR) {
      printed_strings.push_back(std::get<std::string>(args[0].data));
    }
    return Value();
  });
  
  interp.execute();
  
  REQUIRE(printed_strings.size() == 2);
  REQUIRE(printed_strings[0] == "hello");
  REQUIRE(printed_strings[1] == "world");
}

TEST_CASE("ORGASM integration with mixed types", "[orgasm][integration]") {
  std::string source = R"(
.module test
.const i32 42
.const f64 3.14
.const bool true
.import print_i32, core
.import print_f64, core
.import print_bool, core
.start
00:    load_const.i32 const.0
01:    push_param
02:    call import.0
03:    load_const.f64 const.1
04:    push_param
05:    call import.1
06:    load_const.bool const.2
07:    push_param
08:    call import.2
09:    return
.endmodule
)";

  Parser parser(source);
  auto module = parser.parse_module();
  auto shared_module = std::shared_ptr<Module>(std::move(module));
  
  Interpreter interp(shared_module);
  
  int i32_val = 0;
  double f64_val = 0.0;
  bool bool_val = false;
  
  interp.register_import("print_i32", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::I32) {
      i32_val = std::get<int32_t>(args[0].data);
    }
    return Value();
  });
  
  interp.register_import("print_f64", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::F64) {
      f64_val = std::get<double>(args[0].data);
    }
    return Value();
  });
  
  interp.register_import("print_bool", [&](const std::vector<Value> &args) -> Value {
    if (!args.empty() && args[0].type == PrimitiveType::BOOL) {
      bool_val = std::get<bool>(args[0].data);
    }
    return Value();
  });
  
  interp.execute();
  
  REQUIRE(i32_val == 42);
  REQUIRE(f64_val == 3.14);
  REQUIRE(bool_val == true);
}
