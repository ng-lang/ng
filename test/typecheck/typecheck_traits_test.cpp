#include "typecheck_utils.hpp"
#include <module.hpp>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace
{
  struct TraitModuleFixture
  {
    TraitModuleFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
    }

    ~TraitModuleFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
    }

    void write(const Str &name, const Str &source)
    {
      auto ast = parse(source, name);
      REQUIRE(ast != nullptr);
      NG::module::get_module_registry().addModuleInfo(runtime::makert<NG::module::ModuleInfo>(NG::module::ModuleInfo{
          .moduleId = name,
          .moduleName = name,
          .moduleSource = source,
          .moduleAst = ast,
          .moduleAbsolutePath = name,
          .moduleLoadingLocation = "memory",
      }));
    }

    auto paths() const -> Vec<Str>
    {
      return {"[memory]"};
    }
  };

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

  struct SourceModuleFixture
  {
    std::filesystem::path root;
    ScopedEnvVar modulePath;

    SourceModuleFixture()
        : root(std::filesystem::temp_directory_path() /
               ("ng_module_artifact_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))),
          modulePath("NG_MODULE_PATH", root.string())
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
      std::filesystem::create_directories(root);
    }

    ~SourceModuleFixture()
    {
      NG::module::clear_module_loader_cache();
      NG::module::get_module_registry().clear();
      std::filesystem::remove_all(root);
    }

    void write(const std::filesystem::path &relative, const Str &source) const
    {
      auto target = root / relative;
      std::filesystem::create_directories(target.parent_path());
      std::ofstream out{target};
      REQUIRE(out.good());
      out << source;
    }

    auto paths() const -> Vec<Str>
    {
      return {"[force-module-loader]"};
    }
  };
}

TEST_CASE("trait definition and concrete impl should type check", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter {
      label: string;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("Show"));
  check_type_tag(*index["Show"], typeinfo_tag::TRAIT);

  destroyast(ast);
}

TEST_CASE("generic trait bound should allow bounded method calls", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter {
      label: string;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }

    fun render<T: Show>(x: ref<T>) -> string {
      return x.show();
    }

    val c = new Counter { label: "three" };
    val s = render(c);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("s"));
  check_type_tag(*index["s"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("unknown generic trait bound member lookup should not crash",
          "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    fun call<T: Missing>(value: ref<T>) -> unit {
      value.show();
    }
  )");
  REQUIRE(ast != nullptr);
  REQUIRE_NOTHROW(type_check(ast));
  destroyast(ast);
}

TEST_CASE("trait impl missing method should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    type Counter { value: i32; }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
    impl Show for Counter {
    }
  )", "missing method");
}

TEST_CASE("ref trait object should support dynamic dispatch", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { label: string; }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }

    fun render(x: ref<Show>) -> string {
      return x.show();
    }

    val counter = new Counter { label: "three" };
    val view: ref<Show> = counter;
    val text = render(counter);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["view"], typeinfo_tag::REFERENCE);
  check_type_tag(*index["text"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("non object-safe ref trait object should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    trait CloneLike {
      fun clone(self: ref<Self>) -> Self;
    }
    fun copy(x: ref<CloneLike>) -> unit {
    }
  )", "object-safe");
}

TEST_CASE("supertrait bound should expose inherited methods", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { value: i32; }

    trait Eq {
      fun same(self: ref<Self>, other: ref<Self>) -> bool;
    }

    trait Ord: Eq {
      fun less(self: ref<Self>, other: ref<Self>) -> bool;
    }

    impl Ord for Counter {
      fun same(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value == other.value;
      }

      fun less(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value < other.value;
      }
    }

    fun equal<T: Ord>(left: ref<T>, right: ref<T>) -> bool {
      return left.same(right);
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("equal"));
  check_type_tag(*index["Ord"], typeinfo_tag::TRAIT);

  destroyast(ast);
}

TEST_CASE("qualified trait calls should resolve ambiguous trait method names", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Person { name: string; }

    trait Display {
      fun text(self: ref<Self>) -> string;
    }

    trait Debug {
      fun text(self: ref<Self>) -> string;
    }

    impl Display for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Debug for Person {
      fun text(self: ref<Self>) -> string {
        return "debug " + self.name;
      }
    }

    val ada = new Person { name: "Ada" };
    val display = ada.Display::text();
    val debugText = Debug::text(ada);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("display"));
  REQUIRE(index.contains("debugText"));
  check_type_tag(*index["display"], typeinfo_tag::STRING);
  check_type_tag(*index["debugText"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("ambiguous unqualified trait method call should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    type Person { name: string; }

    trait Display {
      fun text(self: ref<Self>) -> string;
    }

    trait Debug {
      fun text(self: ref<Self>) -> string;
    }

    impl Display for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Debug for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    val ada = new Person { name: "Ada" };
    val text = ada.text();
  )", "Ambiguous trait method call");
}

TEST_CASE("inherent method should take precedence over same-named trait method", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Box {
      value: i32;

      fun label(self: ref<Self>) -> string {
        return "inherent";
      }
    }

    trait Label {
      fun label(self: ref<Self>) -> string;
    }

    impl Label for Box {
      fun label(self: ref<Self>) -> string {
        return "trait";
      }
    }

    val box = new Box { value: 1 };
    val inherent = box.label();
    val qualified = box.Label::label();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["inherent"], typeinfo_tag::STRING);
  check_type_tag(*index["qualified"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("cyclic trait inheritance should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    trait A: B {
      fun a(self: ref<Self>) -> unit;
    }

    trait B: A {
      fun b(self: ref<Self>) -> unit;
    }
  )", "Cyclic trait inheritance");
}

TEST_CASE("trait default method may be omitted by impl", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { value: i32; }

    trait Show {
      fun show(self: ref<Self>) -> string;

      fun bracketed(self: ref<Self>) -> string {
        return "[" + self.show() + "]";
      }
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return "counter";
      }
    }

    val c = new Counter { value: 1 };
    val text = c.bracketed();
    val qualified = Show::bracketed(c);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["text"], typeinfo_tag::STRING);
  check_type_tag(*index["qualified"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("trait default method may be overridden by impl", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { value: i32; }

    trait Describe {
      fun describe(self: ref<Self>) -> string {
        return "default";
      }
    }

    impl Describe for Counter {
      fun describe(self: ref<Self>) -> string {
        return "override";
      }
    }

    val c = new Counter { value: 1 };
    val text = c.describe();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["text"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("inherited trait default methods should be available", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { value: i32; }

    trait Eq {
      fun same(self: ref<Self>, other: ref<Self>) -> bool;

      fun not_same(self: ref<Self>, other: ref<Self>) -> bool {
        return !self.same(other);
      }
    }

    trait Ord: Eq {
      fun less(self: ref<Self>, other: ref<Self>) -> bool;

      fun less_or_same(self: ref<Self>, other: ref<Self>) -> bool {
        if (self.less(other)) {
          return true;
        }
        return self.same(other);
      }
    }

    impl Ord for Counter {
      fun same(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value == other.value;
      }

      fun less(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value < other.value;
      }
    }

    val one = new Counter { value: 1 };
    val two = new Counter { value: 2 };
    val a = one.not_same(two);
    val b = one.less_or_same(two);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["a"], typeinfo_tag::BOOL);
  check_type_tag(*index["b"], typeinfo_tag::BOOL);

  destroyast(ast);
}

TEST_CASE("trait default method cannot access concrete Self fields", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    trait Named {
      fun display(self: ref<Self>) -> string {
        return self.label;
      }
    }
  )", "abstract Self");
}

TEST_CASE("duplicate local trait impl should fail", "[TypeCheck][Traits][Coherence][Failure]")
{
  typecheck_failure(R"(
    type Person { name: string; }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Person {
      fun show(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Show for Person {
      fun show(self: ref<Self>) -> string {
        return self.name;
      }
    }
  )", "Duplicate impl");
}

TEST_CASE("overlapping generic and concrete trait impl should fail", "[TypeCheck][Traits][Coherence][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      value: T;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl<T> Show for Box<T> {
      fun show(self: ref<Self>) -> string {
        return "generic";
      }
    }

    impl Show for Box<i32> {
      fun show(self: ref<Self>) -> string {
        return "i32";
      }
    }
  )", "Overlapping impl");
}

TEST_CASE("different traits may be implemented for same type", "[TypeCheck][Traits][Coherence]")
{
  auto ast = parse(R"(
    type Person { name: string; }

    trait Display {
      fun text(self: ref<Self>) -> string;
    }

    trait Debug {
      fun text(self: ref<Self>) -> string;
    }

    impl Display for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Debug for Person {
      fun text(self: ref<Self>) -> string {
        return "debug " + self.name;
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("Person"));

  destroyast(ast);
}

TEST_CASE("same trait may be implemented for different concrete types", "[TypeCheck][Traits][Coherence]")
{
  auto ast = parse(R"(
    type Foo { value: i32; }
    type Bar { value: i32; }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Foo {
      fun show(self: ref<Self>) -> string {
        return "foo";
      }
    }

    impl Show for Bar {
      fun show(self: ref<Self>) -> string {
        return "bar";
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("Foo"));
  REQUIRE(index.contains("Bar"));

  destroyast(ast);
}

TEST_CASE("same trait may be implemented for non-overlapping concrete generic instances", "[TypeCheck][Traits][Coherence]")
{
  auto ast = parse(R"(
    type Box<T> {
      value: T;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Box<i32> {
      fun show(self: ref<Self>) -> string {
        return "i32";
      }
    }

    impl Show for Box<string> {
      fun show(self: ref<Self>) -> string {
        return "string";
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("Box"));

  destroyast(ast);
}

TEST_CASE("equivalent generic trait impl patterns should fail", "[TypeCheck][Traits][Coherence][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      value: T;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl<T> Show for Box<T> {
      fun show(self: ref<Self>) -> string {
        return "first";
      }
    }

    impl<U> Show for Box<U> {
      fun show(self: ref<Self>) -> string {
        return "second";
      }
    }
  )", "Overlapping impl");
}

TEST_CASE("explicit impl selection should allow selected concrete overlap",
          "[TypeCheck][Traits][Coherence][UseImpl]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    type Box<T> {
      property value: T;
    }

    use impl Show for Box<i32>;

    impl<T> Show for Box<T> {
      fun show(self: ref<Self>) -> string {
        return "generic";
      }
    }

    impl Show for Box<i32> {
      fun show(self: ref<Self>) -> string {
        return "i32";
      }
    }

    val box = new Box<i32> { value: 1 };
    val shown = box.show();
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("shown"));
  check_type_tag(*index["shown"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("explicit impl selection should reject missing impl",
          "[TypeCheck][Traits][Coherence][UseImpl][Failure]")
{
  typecheck_failure(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    type Box<T> {
      property value: T;
    }

    use impl Show for Box<i32>;
  )", "Selected impl does not exist");
}

TEST_CASE("abstract type should reject by-value function parameter",
          "[TypeCheck][Traits][Abstract][Failure]")
{
  typecheck_failure(R"(
    type Handle;
    fun consume(value: Handle) -> unit = unit;
  )", "abstract type 'Handle' cannot be used by value");
}

TEST_CASE("abstract type should allow reference function parameter",
          "[TypeCheck][Traits][Abstract]")
{
  auto ast = parse(R"(
    type Handle;
    fun consume(value: ref<Handle>) -> unit = unit;
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("consume"));

  destroyast(ast);
}

TEST_CASE("native opaque type should allow by-value native signatures",
          "[TypeCheck][Traits][Abstract]")
{
  auto ast = parse(R"(
    type NativeHandle = native;
    fun consume(value: NativeHandle) -> unit = native;
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("consume"));

  destroyast(ast);
}

TEST_CASE("trait type should reject by-value function parameter",
          "[TypeCheck][Traits][Abstract][Failure]")
{
  typecheck_failure(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
    fun consume(value: Show) -> unit = unit;
  )", "trait type 'Show' cannot be used by value");
}

TEST_CASE("exported module impl should be visible through wildcard import",
          "[TypeCheck][Traits][ModuleImpl]")
{
  TraitModuleFixture fixture;
  fixture.write("base", R"(
    module base exports *;
    type Box {
      property value: i32;
    }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  fixture.write("impl_a", R"(
    module impl_a exports *;
    import base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "a";
      }
    }
  )");

  auto ast = parse(R"(
    import base (*);
    import impl_a (*);
    val box = new Box { value: 1 };
    val shown = box.show();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, {}, fixture.paths());
  REQUIRE(index.contains("shown"));
  check_type_tag(*index["shown"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("duplicate exported module impls should fail without explicit selection",
          "[TypeCheck][Traits][ModuleImpl][Failure]")
{
  TraitModuleFixture fixture;
  fixture.write("base", R"(
    module base exports *;
    type Box {
      property value: i32;
    }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  fixture.write("impl_a", R"(
    module impl_a exports *;
    import base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "a";
      }
    }
  )");
  fixture.write("impl_b", R"(
    module impl_b exports *;
    import base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "b";
      }
    }
  )");

  auto ast = parse(R"(
    import base (*);
    import impl_a (*);
    import impl_b (*);
    val box = new Box { value: 1 };
    val shown = box.show();
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_THROWS_WITH(type_check(ast, {}, fixture.paths()),
                      Catch::Matchers::ContainsSubstring("Duplicate impl"));

  destroyast(ast);
}

TEST_CASE("module-qualified use impl should select one duplicate exported impl",
          "[TypeCheck][Traits][ModuleImpl][UseImpl]")
{
  TraitModuleFixture fixture;
  fixture.write("base", R"(
    module base exports *;
    type Box {
      property value: i32;
    }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  fixture.write("impl_a", R"(
    module impl_a exports *;
    import base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "a";
      }
    }
  )");
  fixture.write("impl_b", R"(
    module impl_b exports *;
    import base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "b";
      }
    }
  )");

  auto ast = parse(R"(
    use impl impl_a::Show for Box;
    import base (*);
    import impl_a (*);
    import impl_b (*);
    val box = new Box { value: 1 };
    val shown = box.show();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, {}, fixture.paths());
  REQUIRE(index.contains("shown"));
  check_type_tag(*index["shown"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("source module artifacts should load through NG_MODULE_PATH",
          "[TypeCheck][Traits][ModuleArtifact]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/base.ng", R"(
    module pkg.base exports *;
    type Box {
      property value: i32;
    }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  fixture.write("pkg/show_impl.ng", R"(
    module pkg.show_impl exports *;
    import pkg.base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "box";
      }
    }
  )");

  auto ast = parse(R"(
    import pkg.base (*);
    import pkg.show_impl (*);
    val box = new Box { value: 1 };
    val shown = box.show();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, {}, fixture.paths());
  REQUIRE(index.contains("shown"));
  check_type_tag(*index["shown"], typeinfo_tag::STRING);

  auto &registry = NG::module::get_module_registry();
  auto baseArtifact = registry.queryArtifactById("pkg.base");
  REQUIRE(baseArtifact != nullptr);
  REQUIRE(baseArtifact->format == NG::module::ModuleFormat::SourceNg);
  REQUIRE(baseArtifact->originPath.ends_with("pkg/base.ng"));
  REQUIRE(baseArtifact->exports.types.contains("Box"));
  REQUIRE(baseArtifact->exports.types.contains("Show"));
  REQUIRE(baseArtifact->traits.contains("Show"));

  auto implArtifact = registry.queryArtifactById("pkg.show_impl");
  REQUIRE(implArtifact != nullptr);
  REQUIRE(implArtifact->imports.moduleIds == Vec<Str>{"pkg.base"});
  REQUIRE(implArtifact->impls.size() == 1);
  REQUIRE(implArtifact->impls.front().traitName == "Show");
  REQUIRE(implArtifact->impls.front().targetPattern == "Box");
  REQUIRE(implArtifact->impls.front().moduleId == "pkg.show_impl");

  destroyast(ast);
}

TEST_CASE("duplicate source module artifacts should report canonical module ids",
          "[TypeCheck][Traits][ModuleArtifact][Failure]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/base.ng", R"(
    module pkg.base exports *;
    type Box {
      property value: i32;
    }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
  )");
  fixture.write("pkg/impl_a.ng", R"(
    module pkg.impl_a exports *;
    import pkg.base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "a";
      }
    }
  )");
  fixture.write("pkg/impl_b.ng", R"(
    module pkg.impl_b exports *;
    import pkg.base (*);
    export impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "b";
      }
    }
  )");

  auto ast = parse(R"(
    import pkg.base (*);
    import pkg.impl_a (*);
    import pkg.impl_b (*);
    val box = new Box { value: 1 };
    val shown = box.show();
  )");
  REQUIRE(ast != nullptr);

  try
  {
    (void)type_check(ast, {}, fixture.paths());
    FAIL("expected duplicate impl error");
  }
  catch (const TypeCheckingException &ex)
  {
    REQUIRE_THAT(ex.what(), Catch::Matchers::ContainsSubstring("Duplicate impl"));
    REQUIRE_THAT(ex.what(), Catch::Matchers::ContainsSubstring("pkg.impl_a"));
    REQUIRE_THAT(ex.what(), Catch::Matchers::ContainsSubstring("pkg.impl_b"));
  }

  destroyast(ast);
}
