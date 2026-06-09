#include "typecheck_utils.hpp"
#include <typecheck/mangling.hpp>
#include <typecheck/pattern_matching.hpp>

using namespace NG::typecheck;

TEST_CASE("GenericParamType repr and match", "[TypeCheck][GenericTypeInfo]")
{
  GenericParamType plain{"T"};
  GenericParamType bounded{"T", "Display"};
  GenericParamType other{"U"};

  REQUIRE(plain.repr() == "T");
  REQUIRE(bounded.repr() == "T: Display");
  REQUIRE(plain.match(plain));
  REQUIRE(!plain.match(other));
}

TEST_CASE("GenericDefType repr and match", "[TypeCheck][GenericTypeInfo]")
{
  GenericDefType left{"Box", {"T"}, {false}, ASTRef<FunctionDef>{}, {}};
  GenericDefType same{"Box", {"T"}, {false}, ASTRef<FunctionDef>{}, {}};
  GenericDefType different{"Box", {"T", "U"}, {false, false}, ASTRef<FunctionDef>{}, {}};

  REQUIRE(left.repr() == "Box<T>");
  REQUIRE(left.match(same));
  REQUIRE(!left.match(different));
}

TEST_CASE("GenericTypeDef repr and match includes kind", "[TypeCheck][GenericTypeInfo]")
{
  GenericTypeDef objectDef{"Box", {"T"}, {false}, ASTRef<TypeDef>{}, {}};
  GenericTypeDef sameObjectDef{"Box", {"T"}, {false}, ASTRef<TypeDef>{}, {}};
  GenericTypeDef aliasDef{"Box", {"T"}, {false}, ASTRef<TypeAliasDef>{}, {}};

  REQUIRE(objectDef.repr() == "Box<T>");
  REQUIRE(objectDef.match(sameObjectDef));
  REQUIRE(!objectDef.match(aliasDef));
}

TEST_CASE("generic instance symbol mangling is module-scoped and unambiguous", "[TypeCheck][GenericTypeInfo][Mangling]")
{
  auto refVisible = makecheck<ReferenceType>(makecheck<CustomizedType>("Visible", false, false, "app.models"));
  auto boxedI32 = makecheck<CustomizedType>("Box<i32>", false, false, "app.models");
  Vec<CheckingRef<TypeInfo>> args{refVisible, boxedI32};

  auto mangled = mangle_symbol(MangledSymbolKind::Function, "app.main", "classify", args);
  REQUIRE(mangled == "$NG2:v11:F8:app.main8:classify1:224:ref<app.models::Visible>20:app.models::Box<i32>");
  REQUIRE(mangle_symbol(MangledSymbolKind::Function, "other.main", "classify", args) != mangled);
  REQUIRE(mangle_symbol(MangledSymbolKind::Type, "app.main", "classify", args) != mangled);
  REQUIRE(mangle_symbol(MangledSymbolKind::Function, "app.main", "classify.ref", {"Visible"}) !=
          mangle_symbol(MangledSymbolKind::Function, "app.main.classify", "ref", {"Visible"}));
}

TEST_CASE("canonical type names distinguish nominal modules and erase transparent aliases",
          "[TypeCheck][GenericTypeInfo][Mangling]")
{
  auto coreUser = makecheck<CustomizedType>("User", false, false, "core.model");
  auto appUser = makecheck<CustomizedType>("User", false, false, "app.model");
  auto alias = makecheck<TypeAliasType>("UserAlias", coreUser, "app.model");
  auto nativeHandle = makecheck<CustomizedType>("Handle", true, false, "native.io");
  auto userId = makecheck<NewTypeType>("UserId", makecheck<PrimitiveType>(typeinfo_tag::I32), "core.model");

  REQUIRE(canonical_type_name(coreUser) == "core.model::User");
  REQUIRE(canonical_type_name(appUser) == "app.model::User");
  REQUIRE(canonical_type_name(coreUser) != canonical_type_name(appUser));
  REQUIRE(canonical_type_name(alias) == canonical_type_name(coreUser));
  REQUIRE(canonical_type_name(nativeHandle) == "native.io::Handle");
  REQUIRE(canonical_type_name(userId) == "core.model::UserId");
}

TEST_CASE("VarargsType repr and match compare element types", "[TypeCheck][GenericTypeInfo]")
{
  VarargsType pair{Vec<CheckingRef<TypeInfo>>{
      makecheck<PrimitiveType>(typeinfo_tag::I32),
      makecheck<PrimitiveType>(typeinfo_tag::BOOL),
  }};
  VarargsType samePair{Vec<CheckingRef<TypeInfo>>{
      makecheck<PrimitiveType>(typeinfo_tag::I32),
      makecheck<PrimitiveType>(typeinfo_tag::BOOL),
  }};
  VarargsType single{makecheck<PrimitiveType>(typeinfo_tag::I32)};

  REQUIRE(pair.repr() == "Varargs<i32, bool>");
  REQUIRE(pair.match(samePair));
  REQUIRE(!pair.match(single));
}

TEST_CASE("core typeinfo variants preserve transparent and opaque matching", "[TypeCheck][GenericTypeInfo]")
{
  Untyped untyped;
  CustomizedType user{"User"};
  TypeAliasType alias{"Meters", makecheck<PrimitiveType>(typeinfo_tag::I32)};
  NewTypeType newtype{"UserId", makecheck<PrimitiveType>(typeinfo_tag::I32)};
  ReferenceType refI32{makecheck<PrimitiveType>(typeinfo_tag::I32)};
  ReferenceType sameRefI32{makecheck<PrimitiveType>(typeinfo_tag::I32)};
  ReferenceType refBool{makecheck<PrimitiveType>(typeinfo_tag::BOOL)};
  PrimitiveType i32{typeinfo_tag::I32};
  CustomizedType sameUser{"User"};
  CustomizedType otherUser{"Account"};

  REQUIRE(untyped.repr() == "[untyped]");
  REQUIRE(untyped.match(user));
  REQUIRE(user.repr() == "User");
  REQUIRE(user.match(sameUser));
  REQUIRE(!user.match(otherUser));

  REQUIRE(alias.repr() == "Meters");
  REQUIRE(alias.match(i32));
  REQUIRE(alias.match(alias));

  REQUIRE(newtype.repr() == "UserId");
  REQUIRE(newtype.match(newtype));
  REQUIRE(!newtype.match(i32));

  REQUIRE(refI32.repr() == "ref<i32>");
  REQUIRE(refI32.match(sameRefI32));
  REQUIRE(!refI32.match(refBool));
}

TEST_CASE("tagged union, variant, and structural union matching stays nominal", "[TypeCheck][GenericTypeInfo]")
{
  auto i32 = makecheck<PrimitiveType>(typeinfo_tag::I32);
  auto str = makecheck<PrimitiveType>(typeinfo_tag::STRING);

  TaggedUnionType result{"Result"};
  result.variants["Ok"] = {i32};

  VariantType ok{"Result", "Ok", 0, {i32}};
  VariantType err{"Result", "Err", 1, {str}};
  TaggedUnionType other{"Other"};
  other.variants["Ok"] = {i32};

  UnionType either{Vec<CheckingRef<TypeInfo>>{i32, str}};

  REQUIRE(result.repr().find("Result = ") == 0);
  REQUIRE(result.repr().find("Ok(i32)") != Str::npos);
  REQUIRE(result.match(ok));
  REQUIRE(!result.match(other));

  REQUIRE(ok.repr() == "Ok(i32)");
  REQUIRE(ok.match(result));
  REQUIRE(ok.match(ok));
  REQUIRE(!ok.match(err));

  REQUIRE(either.repr() == "i32 | string");
  REQUIRE(either.match(*i32));
  REQUIRE(either.match(*str));
  REQUIRE(!either.match(result));
}

TEST_CASE("type pattern matching parses nested structured instance arguments", "[TypeCheck][GenericTypeInfo]")
{
  auto pattern = std::make_shared<TypeAnnotation>("Box");
  pattern->type = TypeAnnotationType::CUSTOMIZED;
  pattern->genericArgs.push_back(std::make_shared<TypeAnnotation>("T"));

  Map<Str, CheckingRef<TypeInfo>> bindings;
  REQUIRE(typePatternMatch(pattern.get(), makecheck<CustomizedType>("Box<ref<vector<i32>>>"), Set<Str>{"T"}, bindings));
  REQUIRE(bindings.contains("T"));

  auto ref = std::dynamic_pointer_cast<ReferenceType>(bindings.at("T"));
  REQUIRE(ref != nullptr);
  auto vector = std::dynamic_pointer_cast<VectorType>(ref->referencedType);
  REQUIRE(vector != nullptr);
  REQUIRE(vector->elementType->tag() == typeinfo_tag::I32);
}
