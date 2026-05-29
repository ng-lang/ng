#include "../test.hpp"

#include <intp/intp.hpp>
#include <intp/runtime_numerals.hpp>
#include <module.hpp>
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <runtime/native_marshaling.hpp>
#include <typecheck/typecheck.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace NG;
using namespace NG::runtime;
using namespace NG::typecheck;

namespace fs = std::filesystem;

namespace
{
  struct ScopedEnvVar
  {
    Str name;
    Str previous;
    bool hadPrevious = false;

    ScopedEnvVar(Str name, const Str &value) : name(std::move(name))
    {
      if (const char *existing = std::getenv(this->name.c_str()))
      {
        previous = existing;
        hadPrevious = true;
      }
#ifdef _WIN32
      _putenv_s(this->name.c_str(), value.c_str());
#else
      setenv(this->name.c_str(), value.c_str(), 1);
#endif
    }

    ~ScopedEnvVar()
    {
#ifdef _WIN32
      _putenv_s(name.c_str(), hadPrevious ? previous.c_str() : "");
#else
      if (hadPrevious)
      {
        setenv(name.c_str(), previous.c_str(), 1);
      }
      else
      {
        unsetenv(name.c_str());
      }
#endif
    }
  };

  struct NativeSourceFixture
  {
    fs::path root;
    ScopedEnvVar modulePath;

    NativeSourceFixture()
        : root(fs::temp_directory_path() /
               ("ng_native_module_artifact_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))),
          modulePath("NG_MODULE_PATH", root.string())
    {
      NG::module::clear_module_loader_cache();
      fs::create_directories(root);
    }

    ~NativeSourceFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
      fs::remove_all(root);
    }

    void write(const fs::path &relative, const Str &source) const
    {
      auto target = root / relative;
      fs::create_directories(target.parent_path());
      std::ofstream out{target};
      REQUIRE(out.good());
      out << source;
    }
  };

  struct NativeModuleFixture
  {
    NativeModuleFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
      registerNativeMath();
    }

    ~NativeModuleFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
    }

    static auto nativeIncHandler() -> NGCallable
    {
      return [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell> {
        auto value = NG::runtime::native::require_numeric_arg<int32_t>(
            "nativeInc", NG::runtime::native::native_args_view(context, args), 0, "an integer");
        return numeral_cell_from_value<int32_t>(value + 1);
      };
    }

    static void registerNativeMath()
    {
      auto i32 = makecheck<PrimitiveType>(typeinfo_tag::I32);
      auto descriptor = makert<NG::module::NativeModuleDescriptor>();
      descriptor->moduleId = "native_math";
      descriptor->functions.insert_or_assign("nativeInc", nativeIncHandler());
      descriptor->typeIndex.insert_or_assign(
          "nativeInc", makecheck<FunctionType>(i32, Vec<CheckingRef<TypeInfo>>{i32}));
      descriptor->typeIndex.insert_or_assign(
          "NativeBox", makecheck<CustomizedType>("NativeBox", true, false, "native_math"));
      descriptor->exports.declared.insert("*");
      descriptor->requireSignatures = true;
      NG::module::get_module_registry().registerNativeModuleDescriptor(descriptor);
    }
  };
} // namespace

TEST_CASE("native module descriptor publishes an importable type artifact",
          "[ModuleArtifact][Native][TypeCheck]")
{
  NativeModuleFixture fixture;
  auto ast = parse(R"(
    import native_math (nativeInc, NativeBox);

    val box: NativeBox = unit;
    val value: i32 = nativeInc(41);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("box"));
  REQUIRE(index.contains("value"));
  REQUIRE(index["value"]->tag() == typeinfo_tag::I32);

  auto artifact = NG::module::get_module_registry().queryArtifactById("native_math");
  REQUIRE(artifact != nullptr);
  REQUIRE(artifact->format == NG::module::ModuleFormat::Native);
  REQUIRE(artifact->exports.types.contains("nativeInc"));
  REQUIRE(artifact->exports.types.contains("NativeBox"));

  destroyast(ast);
}

TEST_CASE("native module descriptor rejects handlers without NG signatures",
          "[ModuleArtifact][Native][Failure]")
{
  NG::module::clear_module_loader_cache();
  NG::module::get_module_registry().clear();
  auto descriptor = makert<NG::module::NativeModuleDescriptor>();
  descriptor->moduleId = "native_bad";
  descriptor->functions.insert_or_assign("missingSignature", NativeModuleFixture::nativeIncHandler());
  descriptor->requireSignatures = true;

  REQUIRE_THROWS_WITH(
      NG::module::get_module_registry().registerNativeModuleDescriptor(descriptor),
      Catch::Matchers::ContainsSubstring(
          "Native module descriptor missing NG signature for function: native_bad::missingSignature"));
}

TEST_CASE("STUPID imports native module artifacts without a source file",
          "[ModuleArtifact][Native][Interpreter]")
{
  NativeModuleFixture fixture;
  auto ast = parse(R"(
    import native_math (nativeInc);
    assert(nativeInc(41) == 42);
  )");
  REQUIRE(ast != nullptr);
  auto prelude = build_prelude_type_index();
  type_check(ast, prelude);

  auto interpreter = std::unique_ptr<NG::intp::Interpreter>(NG::intp::stupid());
  ast->accept(interpreter.get());

  destroyast(ast);
}

TEST_CASE("native descriptors preserve source stub helper functions",
          "[ModuleArtifact][Native][TypeCheck][Interpreter]")
{
  NativeModuleFixture fixture;
  NativeSourceFixture source;
  source.write("native_math.ng", R"(
    module native_math exports *;

    fun nativeInc(value: i32) -> i32 = native;

    fun doubleInc(value: i32) -> i32 {
      return nativeInc(value) + 1;
    }
  )");

  auto ast = parse(R"(
    import native_math (doubleInc);

    val value: i32 = doubleInc(40);
    assert(value == 42);
  )");
  REQUIRE(ast != nullptr);

  auto prelude = build_prelude_type_index();
  REQUIRE_NOTHROW(type_check(ast, prelude, Vec<Str>{source.root.string()}));

  auto interpreter = std::unique_ptr<NG::intp::Interpreter>(NG::intp::stupid());
  ast->accept(interpreter.get());

  destroyast(ast);
}

TEST_CASE("ORGASM imports native module artifacts as external native calls",
          "[ModuleArtifact][Native][Orgasm]")
{
  NativeModuleFixture fixture;
  auto ast = parse(R"(
    import native_math (nativeInc);
    assert(nativeInc(41) == 42);
  )");
  REQUIRE(ast != nullptr);

  auto compilerAst = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compilerAst != nullptr);
  NG::orgasm::Compiler compiler{{}, Vec<Str>{"nativeInc"}};
  auto bytecode = compiler.compile(compilerAst);

  NG::orgasm::VM vm;
  vm.register_native_raw("nativeInc", [](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
    if (args.empty())
    {
      throw RuntimeException("nativeInc requires an argument");
    }
    return numeral_cell_from_value<int32_t>(read_inline_cell_bytes<int32_t>(args[0]) + 1);
  });
  vm.run(bytecode);

  destroyast(ast);
}
