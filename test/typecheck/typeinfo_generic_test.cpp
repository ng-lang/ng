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
