#include "../test.hpp"
#include <chrono>
#include <filesystem>
#include <orgasm/module.hpp>

using namespace NG::orgasm;

namespace
{
auto read_u16(const Vec<uint8_t> &code, size_t offset) -> uint16_t
{
  return static_cast<uint16_t>(code[offset]) | (static_cast<uint16_t>(code[offset + 1]) << 8);
}

void append_u16(Vec<uint8_t> &code, uint16_t value)
{
  code.push_back(static_cast<uint8_t>(value & 0xFF));
  code.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void append_u32(Vec<uint8_t> &code, uint32_t value)
{
  code.push_back(static_cast<uint8_t>(value & 0xFF));
  code.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  code.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  code.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void append_u64(Vec<uint8_t> &code, uint64_t value)
{
  for (int shift = 0; shift < 64; shift += 8)
  {
    code.push_back(static_cast<uint8_t>((value >> shift) & 0xFF));
  }
}
} // namespace

TEST_CASE("bytecode module artifacts round trip module metadata", "[OrgasmTest][Module][Ngo]")
{
  BytecodeModule module;
  module.name = "pkg.sample";
  module.sourceHash = "source-hash";
  module.constants = {1, 2};
  module.float_constants = {1.5};
  module.strings = {"hello"};
  module.imports.push_back(ExternalSymbol{.moduleName = "pkg.dep", .symbolName = "answer"});
  module.exports["main"] = 0;
  module.exportTypeReprs["main"] = "fun () -> i32";
  module.traitMetadata.push_back(BytecodeTraitMetadata{
      .name = "Show",
      .moduleId = "pkg.sample",
      .typeParamNames = {"T"},
      .superTraits = {"Debug"},
      .methods = {{"show", "fun (ref<Self>) -> string"}},
      .allMethods = {{"show", "fun (ref<Self>) -> string"}},
  });
  module.implMetadata.push_back(BytecodeImplMetadata{
      .traitName = "Show",
      .targetPattern = "Point",
      .moduleId = "pkg.sample",
      .genericParamNames = {"T"},
      .whereBounds = {"T: Debug"},
      .methods = {{"show", "Point.Show::show"}},
  });
  module.types.push_back(Type{
      .name = "Point",
      .properties = {"x", "y"},
      .derivedTraits = {"Clone"},
      .variants = {Variant{.name = "Some", .payloadFields = {"value"}}},
  });

  Function main;
  main.name = "main";
  main.num_locals = 1;
  main.num_params = 0;
  main.explicit_receiver = true;
  main.code = {static_cast<uint8_t>(OpCode::PUSH_I32), 42, 0, 0, 0, static_cast<uint8_t>(OpCode::RETURN)};
  module.functions.push_back(std::move(main));

  auto path = std::filesystem::temp_directory_path() /
              ("ng_roundtrip_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
               ".ngo");
  write_bytecode_module(module, path.string(), "hash");

  auto loaded = read_bytecode_module(path.string(), "pkg.sample");
  REQUIRE(loaded.name == "pkg.sample");
  REQUIRE(loaded.sourceHash == "hash");
  REQUIRE(loaded.constants == Vec<int64_t>{1, 2});
  REQUIRE(loaded.float_constants == Vec<double>{1.5});
  REQUIRE(loaded.strings == Vec<Str>{"hello"});
  REQUIRE(loaded.imports.size() == 1);
  REQUIRE(loaded.imports[0].moduleName == "pkg.dep");
  REQUIRE(loaded.exports.at("main") == 0);
  REQUIRE(loaded.exportTypeReprs.at("main") == "fun () -> i32");
  REQUIRE(loaded.traitMetadata.size() == 1);
  REQUIRE(loaded.traitMetadata[0].name == "Show");
  REQUIRE(loaded.traitMetadata[0].superTraits == Vec<Str>{"Debug"});
  REQUIRE(loaded.traitMetadata[0].methods.at("show") == "fun (ref<Self>) -> string");
  REQUIRE(loaded.implMetadata.size() == 1);
  REQUIRE(loaded.implMetadata[0].traitName == "Show");
  REQUIRE(loaded.implMetadata[0].methods.at("show") == "Point.Show::show");
  REQUIRE(loaded.types.size() == 1);
  REQUIRE(loaded.types[0].derivedTraits == Vec<Str>{"Clone"});
  REQUIRE(loaded.types[0].variants[0].payloadFields == Vec<Str>{"value"});
  REQUIRE(loaded.functions.size() == 1);
  REQUIRE(loaded.functions[0].explicit_receiver);
  REQUIRE(loaded.functions[0].code == module.functions[0].code);
  REQUIRE_THROWS_WITH(read_bytecode_module(path.string(), "pkg.other"),
                      Catch::Matchers::ContainsSubstring("Bytecode module id mismatch"));

  std::filesystem::remove(path);
}

TEST_CASE("bytecode module merge remaps tagged union type operands", "[OrgasmTest][Module]")
{
  BytecodeModule base;
  base.types.push_back(Type{.name = "Base"});

  BytecodeModule other;
  other.types.push_back(Type{.name = "Result", .variants = Vec<Variant>{{.name = "Ok"}}});

  Function fn;
  fn.name = "make_result";
  fn.code = {
      static_cast<uint8_t>(OpCode::CONSTRUCT_TAGGED),
      0, 0, // type index
      0, 0, // variant index
      0, 0, // payload count
      static_cast<uint8_t>(OpCode::RETURN),
  };
  other.functions.push_back(std::move(fn));

  base.merge(other);

  REQUIRE(base.types.size() == 2);
  REQUIRE(base.functions.size() == 1);
  REQUIRE(read_u16(base.functions[0].code, 1) == 1);
  REQUIRE(read_u16(base.functions[0].code, 3) == 0);
  REQUIRE(read_u16(base.functions[0].code, 5) == 0);
}

TEST_CASE("bytecode module merge remaps mixed operands and prefixes exports", "[OrgasmTest][Module]")
{
  BytecodeModule base;
  base.constants = {10, 20};
  base.strings = {"existing", "member"};
  base.functions.push_back(Function{.name = "base_fn", .code = {static_cast<uint8_t>(OpCode::RETURN)}});
  base.types.push_back(Type{.name = "BaseType"});

  BytecodeModule other;
  other.constants = {30};
  other.strings = {"field", "dynamic", "nativeFn", "WrappedName", "Show"};
  other.types.push_back(Type{.name = "Result", .variants = Vec<Variant>{{.name = "Ok"}}});
  other.types.push_back(Type{.name = "Wrapped"});

  Function callee;
  callee.name = "callee";
  callee.code = {static_cast<uint8_t>(OpCode::RETURN)};
  other.functions.push_back(callee);

  Function caller;
  caller.name = "caller";
  auto &code = caller.code;

  auto emit_u16_op = [&](OpCode op, uint16_t operand) -> size_t {
    const size_t pos = code.size();
    code.push_back(static_cast<uint8_t>(op));
    append_u16(code, operand);
    return pos;
  };
  auto emit_u16_u16_op = [&](OpCode op, uint16_t left, uint16_t right) -> size_t {
    const size_t pos = code.size();
    code.push_back(static_cast<uint8_t>(op));
    append_u16(code, left);
    append_u16(code, right);
    return pos;
  };
  auto emit_u16_u16_u16_op = [&](OpCode op, uint16_t a, uint16_t b, uint16_t c) -> size_t {
    const size_t pos = code.size();
    code.push_back(static_cast<uint8_t>(op));
    append_u16(code, a);
    append_u16(code, b);
    append_u16(code, c);
    return pos;
  };

  const size_t loadStrPos = emit_u16_op(OpCode::LOAD_STR, 0);
  const size_t instanceOfPos = emit_u16_op(OpCode::INSTANCE_OF, 1);
  const size_t setPropertyPos = emit_u16_op(OpCode::SET_PROPERTY, 7);
  const size_t getPropertyPos = emit_u16_op(OpCode::GET_PROPERTY, 8);
  const size_t setPropertyStrPos = emit_u16_op(OpCode::SET_PROPERTY_STR, 2);
  const size_t getPropertyStrPos = emit_u16_op(OpCode::GET_PROPERTY_STR, 3);
  const size_t invokeMemberPos = emit_u16_u16_op(OpCode::INVOKE_MEMBER, 1, 2);
  const size_t nativeCallPos = emit_u16_u16_op(OpCode::NATIVE_CALL, 2, 1);
  const size_t wrapNewtypePos = emit_u16_op(OpCode::WRAP_NEWTYPE, 3);
  const size_t makeTraitRefPos = emit_u16_op(OpCode::MAKE_TRAIT_REF, 4);
  const size_t loadConstPos = emit_u16_op(OpCode::LOAD_CONST, 0);
  const size_t callPos = emit_u16_u16_op(OpCode::CALL, 0, 1);
  const size_t constructTaggedPos = emit_u16_u16_u16_op(OpCode::CONSTRUCT_TAGGED, 0, 0, 1);
  const size_t loadLocalPos = emit_u16_op(OpCode::LOAD_LOCAL, 9);
  const size_t loadParamPos = emit_u16_op(OpCode::LOAD_PARAM, 10);
  const size_t storeLocalPos = emit_u16_op(OpCode::STORE_LOCAL, 11);
  const size_t loadGlobalPos = emit_u16_op(OpCode::LOAD_GLOBAL, 12);
  const size_t storeGlobalPos = emit_u16_op(OpCode::STORE_GLOBAL, 13);

  code.push_back(static_cast<uint8_t>(OpCode::JUMP));
  append_u32(code, 1234);
  code.push_back(static_cast<uint8_t>(OpCode::JUMP_IF_FALSE));
  append_u32(code, 5678);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_I64));
  append_u64(code, 111);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_U64));
  append_u64(code, 222);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_I32));
  append_u32(code, 333);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_U32));
  append_u32(code, 444);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_F32));
  append_u32(code, 0x3F800000);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_I16));
  append_u16(code, 12);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_U16));
  append_u16(code, 13);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_I8));
  code.push_back(14);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_U8));
  code.push_back(15);
  code.push_back(static_cast<uint8_t>(OpCode::PUSH_BOOL));
  code.push_back(1);

  code.push_back(static_cast<uint8_t>(OpCode::NEW_TUPLE_SPREAD));
  append_u16(code, 3);
  code.push_back(1);
  code.push_back(0);
  code.push_back(1);
  code.push_back(static_cast<uint8_t>(OpCode::NEW_ARRAY_SPREAD));
  append_u16(code, 2);
  code.push_back(0);
  code.push_back(1);

  emit_u16_op(OpCode::NEW_OBJECT, 4);
  emit_u16_op(OpCode::NEW_ARRAY, 5);
  emit_u16_op(OpCode::NEW_TUPLE, 6);
  emit_u16_op(OpCode::PRINT, 7);
  emit_u16_op(OpCode::CALL_IMPORT, 8);
  code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  other.functions.push_back(caller);
  other.exports["run"] = 1;

  base.merge(other, "math");

  REQUIRE(base.constants == Vec<int64_t>{10, 20, 30});
  REQUIRE(base.strings == Vec<Str>{"existing", "member", "field", "dynamic", "nativeFn", "WrappedName", "Show"});
  REQUIRE(base.functions.size() == 3);
  REQUIRE(base.types.size() == 3);
  REQUIRE(base.exports.at("math.run") == 2);

  const auto &merged = base.functions[2].code;
  REQUIRE(read_u16(merged, loadStrPos + 1) == 2);
  REQUIRE(read_u16(merged, instanceOfPos + 1) == 3);
  REQUIRE(read_u16(merged, setPropertyPos + 1) == 7);
  REQUIRE(read_u16(merged, getPropertyPos + 1) == 8);
  REQUIRE(read_u16(merged, setPropertyStrPos + 1) == 4);
  REQUIRE(read_u16(merged, getPropertyStrPos + 1) == 5);
  REQUIRE(read_u16(merged, invokeMemberPos + 1) == 3);
  REQUIRE(read_u16(merged, invokeMemberPos + 3) == 2);
  REQUIRE(read_u16(merged, nativeCallPos + 1) == 4);
  REQUIRE(read_u16(merged, nativeCallPos + 3) == 1);
  REQUIRE(read_u16(merged, wrapNewtypePos + 1) == 5);
  REQUIRE(read_u16(merged, makeTraitRefPos + 1) == 6);
  REQUIRE(read_u16(merged, loadConstPos + 1) == 2);
  REQUIRE(read_u16(merged, callPos + 1) == 1);
  REQUIRE(read_u16(merged, callPos + 3) == 1);
  REQUIRE(read_u16(merged, constructTaggedPos + 1) == 1);
  REQUIRE(read_u16(merged, constructTaggedPos + 3) == 0);
  REQUIRE(read_u16(merged, constructTaggedPos + 5) == 1);
  REQUIRE(read_u16(merged, loadLocalPos + 1) == 9);
  REQUIRE(read_u16(merged, loadParamPos + 1) == 10);
  REQUIRE(read_u16(merged, storeLocalPos + 1) == 11);
  REQUIRE(read_u16(merged, loadGlobalPos + 1) == 12);
  REQUIRE(read_u16(merged, storeGlobalPos + 1) == 13);
}
