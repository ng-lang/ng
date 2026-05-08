#include "typecheck_utils.hpp"

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
