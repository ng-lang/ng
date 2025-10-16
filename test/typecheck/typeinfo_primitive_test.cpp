#include "typecheck_utils.hpp"

void check_by_order(Vec<CheckingRef<PrimitiveType>> &&vecs)
{
  for (size_t i = 0; i < vecs.size(); i++)
  {
    for (size_t j = 0; j <= i; j++)
    {
      REQUIRE(vecs[i]->match(*vecs[j]));
    }
  }
}

TEST_CASE("should run primitive checks via string", "[Type][Checking][Primitive]")
{
  auto byteType = PrimitiveType::from("byte");
  auto shortType = PrimitiveType::from("short");
  auto intType = PrimitiveType::from("int");
  auto longType = PrimitiveType::from("long");
  auto ubyteType = PrimitiveType::from("ubyte");
  auto ushortType = PrimitiveType::from("ushort");
  auto uintType = PrimitiveType::from("uint");
  auto ulongType = PrimitiveType::from("ulong");
  auto halfType = PrimitiveType::from("half");
  auto floatType = PrimitiveType::from("float");
  auto doubleType = PrimitiveType::from("double");
  auto quadrupleType = PrimitiveType::from("quadruple");
  auto iptrType = PrimitiveType::from("iptr");
  auto uptrType = PrimitiveType::from("uptr");
  auto i8Type = PrimitiveType::from("i8");
  auto u8Type = PrimitiveType::from("u8");
  auto i16Type = PrimitiveType::from("i16");
  auto u16Type = PrimitiveType::from("u16");
  auto i32Type = PrimitiveType::from("i32");
  auto u32Type = PrimitiveType::from("u32");
  auto i64Type = PrimitiveType::from("i64");
  auto u64Type = PrimitiveType::from("u64");
  auto f16Type = PrimitiveType::from("f16");
  auto f32Type = PrimitiveType::from("f32");
  auto f64Type = PrimitiveType::from("f64");
  auto f128Type = PrimitiveType::from("f128");

  check_by_order(Vec<CheckingRef<PrimitiveType>>{byteType, shortType, intType, longType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{byteType, shortType, intType, iptrType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, ulongType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, uptrType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{halfType, floatType, doubleType, quadrupleType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{i8Type, i16Type, i32Type, i64Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{u8Type, u16Type, u32Type, u64Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{f16Type, f32Type, f64Type, f128Type});

  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, i16Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, i32Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, i64Type});
}

TEST_CASE("should run primitive checks via type annotation type", "[Type][Checking][Primitive]")
{
  auto byteType = PrimitiveType::from(TypeAnnotationType::BUILTIN_BYTE);
  auto shortType = PrimitiveType::from(TypeAnnotationType::BUILTIN_SHORT);
  auto intType = PrimitiveType::from(TypeAnnotationType::BUILTIN_INT);
  auto longType = PrimitiveType::from(TypeAnnotationType::BUILTIN_LONG);
  auto ubyteType = PrimitiveType::from(TypeAnnotationType::BUILTIN_UBYTE);
  auto ushortType = PrimitiveType::from(TypeAnnotationType::BUILTIN_USHORT);
  auto uintType = PrimitiveType::from(TypeAnnotationType::BUILTIN_UINT);
  auto ulongType = PrimitiveType::from(TypeAnnotationType::BUILTIN_ULONG);
  auto halfType = PrimitiveType::from(TypeAnnotationType::BUILTIN_HALF);
  auto floatType = PrimitiveType::from(TypeAnnotationType::BUILTIN_FLOAT);
  auto doubleType = PrimitiveType::from(TypeAnnotationType::BUILTIN_DOUBLE);
  auto quadrupleType = PrimitiveType::from(TypeAnnotationType::BUILTIN_QUADRUPLE);
  auto iptrType = PrimitiveType::from(TypeAnnotationType::BUILTIN_IPTR);
  auto uptrType = PrimitiveType::from(TypeAnnotationType::BUILTIN_UPTR);
  auto i8Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_I8);
  auto u8Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_U8);
  auto i16Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_I16);
  auto u16Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_U16);
  auto i32Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_I32);
  auto u32Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_U32);
  auto i64Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_I64);
  auto u64Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_U64);
  auto f16Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_F16);
  auto f32Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_F32);
  auto f64Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_F64);
  auto f128Type = PrimitiveType::from(TypeAnnotationType::BUILTIN_F128);

  check_by_order(Vec<CheckingRef<PrimitiveType>>{byteType, shortType, intType, longType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{byteType, shortType, intType, iptrType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, ulongType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, uptrType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{halfType, floatType, doubleType, quadrupleType});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{i8Type, i16Type, i32Type, i64Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{u8Type, u16Type, u32Type, u64Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{f16Type, f32Type, f64Type, f128Type});

  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, i16Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, i32Type});
  check_by_order(Vec<CheckingRef<PrimitiveType>>{ubyteType, ushortType, uintType, i64Type});
}