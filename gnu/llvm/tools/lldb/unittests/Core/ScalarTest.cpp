//===-- ScalarTest.cpp ------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "gtest/gtest.h"

#include "lldb/Core/Scalar.h"
#include "lldb/Utility/DataExtractor.h"
#include "lldb/Utility/Endian.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/StreamString.h"
#include "llvm/Testing/Support/Error.h"

using namespace lldb_private;
using namespace llvm;

TEST(ScalarTest, RightShiftOperator) {
  int a = 0x00001000;
  int b = 0xFFFFFFFF;
  int c = 4;
  Scalar a_scalar(a);
  Scalar b_scalar(b);
  Scalar c_scalar(c);
  ASSERT_EQ(a >> c, a_scalar >> c_scalar);
  ASSERT_EQ(b >> c, b_scalar >> c_scalar);
}

TEST(ScalarTest, GetBytes) {
  int a = 0x01020304;
  long long b = 0x0102030405060708LL;
  float c = 1234567.89e32f;
  double d = 1234567.89e42;
  char e[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  char f[32] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
  Scalar a_scalar(a);
  Scalar b_scalar(b);
  Scalar c_scalar(c);
  Scalar d_scalar(d);
  Scalar e_scalar;
  Scalar f_scalar;
  DataExtractor e_data(e, sizeof(e), endian::InlHostByteOrder(),
                       sizeof(void *));
  Status e_error =
      e_scalar.SetValueFromData(e_data, lldb::eEncodingUint, sizeof(e));
  DataExtractor f_data(f, sizeof(f), endian::InlHostByteOrder(),
                       sizeof(void *));
  Status f_error =
      f_scalar.SetValueFromData(f_data, lldb::eEncodingUint, sizeof(f));
  ASSERT_EQ(0, memcmp(&a, a_scalar.GetBytes(), sizeof(a)));
  ASSERT_EQ(0, memcmp(&b, b_scalar.GetBytes(), sizeof(b)));
  ASSERT_EQ(0, memcmp(&c, c_scalar.GetBytes(), sizeof(c)));
  ASSERT_EQ(0, memcmp(&d, d_scalar.GetBytes(), sizeof(d)));
  ASSERT_EQ(0, e_error.Fail());
  ASSERT_EQ(0, memcmp(e, e_scalar.GetBytes(), sizeof(e)));
  ASSERT_EQ(0, f_error.Fail());
  ASSERT_EQ(0, memcmp(f, f_scalar.GetBytes(), sizeof(f)));
}

TEST(ScalarTest, CastOperations) {
  long long a = 0xf1f2f3f4f5f6f7f8LL;
  Scalar a_scalar(a);
  ASSERT_EQ((signed char)a, a_scalar.SChar());
  ASSERT_EQ((unsigned char)a, a_scalar.UChar());
  ASSERT_EQ((signed short)a, a_scalar.SShort());
  ASSERT_EQ((unsigned short)a, a_scalar.UShort());
  ASSERT_EQ((signed int)a, a_scalar.SInt());
  ASSERT_EQ((unsigned int)a, a_scalar.UInt());
  ASSERT_EQ((signed long)a, a_scalar.SLong());
  ASSERT_EQ((unsigned long)a, a_scalar.ULong());
  ASSERT_EQ((signed long long)a, a_scalar.SLongLong());
  ASSERT_EQ((unsigned long long)a, a_scalar.ULongLong());

  int a2 = 23;
  Scalar a2_scalar(a2);
  ASSERT_EQ((float)a2, a2_scalar.Float());
  ASSERT_EQ((double)a2, a2_scalar.Double());
  ASSERT_EQ((long double)a2, a2_scalar.LongDouble());
}

TEST(ScalarTest, ExtractBitfield) {
  uint32_t len = sizeof(long long) * 8;

  long long a1 = 0xf1f2f3f4f5f6f7f8LL;
  long long b1 = 0xff1f2f3f4f5f6f7fLL;
  Scalar s_scalar(a1);
  ASSERT_TRUE(s_scalar.ExtractBitfield(0, 0));
  ASSERT_EQ(0, memcmp(&a1, s_scalar.GetBytes(), sizeof(a1)));
  ASSERT_TRUE(s_scalar.ExtractBitfield(len, 0));
  ASSERT_EQ(0, memcmp(&a1, s_scalar.GetBytes(), sizeof(a1)));
  ASSERT_TRUE(s_scalar.ExtractBitfield(len - 4, 4));
  ASSERT_EQ(0, memcmp(&b1, s_scalar.GetBytes(), sizeof(b1)));

  unsigned long long a2 = 0xf1f2f3f4f5f6f7f8ULL;
  unsigned long long b2 = 0x0f1f2f3f4f5f6f7fULL;
  Scalar u_scalar(a2);
  ASSERT_TRUE(u_scalar.ExtractBitfield(0, 0));
  ASSERT_EQ(0, memcmp(&a2, u_scalar.GetBytes(), sizeof(a2)));
  ASSERT_TRUE(u_scalar.ExtractBitfield(len, 0));
  ASSERT_EQ(0, memcmp(&a2, u_scalar.GetBytes(), sizeof(a2)));
  ASSERT_TRUE(u_scalar.ExtractBitfield(len - 4, 4));
  ASSERT_EQ(0, memcmp(&b2, u_scalar.GetBytes(), sizeof(b2)));
}

template <typename T> static std::string ScalarGetValue(T value) {
  StreamString stream;
  Scalar(value).GetValue(&stream, false);
  return stream.GetString();
}

TEST(ScalarTest, GetValue) {
  EXPECT_EQ("12345", ScalarGetValue<signed short>(12345));
  EXPECT_EQ("-12345", ScalarGetValue<signed short>(-12345));
  EXPECT_EQ("12345", ScalarGetValue<unsigned short>(12345));
  EXPECT_EQ(std::to_string(std::numeric_limits<unsigned short>::max()),
            ScalarGetValue(std::numeric_limits<unsigned short>::max()));

  EXPECT_EQ("12345", ScalarGetValue<signed int>(12345));
  EXPECT_EQ("-12345", ScalarGetValue<signed int>(-12345));
  EXPECT_EQ("12345", ScalarGetValue<unsigned int>(12345));
  EXPECT_EQ(std::to_string(std::numeric_limits<unsigned int>::max()),
            ScalarGetValue(std::numeric_limits<unsigned int>::max()));

  EXPECT_EQ("12345678", ScalarGetValue<signed long>(12345678L));
  EXPECT_EQ("-12345678", ScalarGetValue<signed long>(-12345678L));
  EXPECT_EQ("12345678", ScalarGetValue<unsigned long>(12345678UL));
  EXPECT_EQ(std::to_string(std::numeric_limits<unsigned long>::max()),
            ScalarGetValue(std::numeric_limits<unsigned long>::max()));

  EXPECT_EQ("1234567890123", ScalarGetValue<signed long long>(1234567890123LL));
  EXPECT_EQ("-1234567890123",
            ScalarGetValue<signed long long>(-1234567890123LL));
  EXPECT_EQ("1234567890123",
            ScalarGetValue<unsigned long long>(1234567890123ULL));
  EXPECT_EQ(std::to_string(std::numeric_limits<unsigned long long>::max()),
            ScalarGetValue(std::numeric_limits<unsigned long long>::max()));
}

TEST(ScalarTest, Division) {
  Scalar lhs(5.0);
  Scalar rhs(2.0);
  Scalar r = lhs / rhs;
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(r, Scalar(2.5));
}

TEST(ScalarTest, Promotion) {
  static Scalar::Type int_types[] = {
      Scalar::e_sint,    Scalar::e_uint,      Scalar::e_slong,
      Scalar::e_ulong,   Scalar::e_slonglong, Scalar::e_ulonglong,
      Scalar::e_sint128, Scalar::e_uint128,   Scalar::e_sint256,
      Scalar::e_uint256,
      Scalar::e_void // sentinel
  };

  static Scalar::Type float_types[] = {
      Scalar::e_float, Scalar::e_double, Scalar::e_long_double,
      Scalar::e_void // sentinel
  };

  for (int i = 0; int_types[i] != Scalar::e_void; ++i) {
    for (int j = 0; float_types[j] != Scalar::e_void; ++j) {
      Scalar lhs(2);
      EXPECT_TRUE(lhs.Promote(int_types[i])) << "int promotion #" << i;
      Scalar rhs(0.5f);
      EXPECT_TRUE(rhs.Promote(float_types[j])) << "float promotion #" << j;
      Scalar x(2.5f);
      EXPECT_TRUE(x.Promote(float_types[j]));
      EXPECT_EQ(lhs + rhs, x);
    }
  }

  for (int i = 0; float_types[i] != Scalar::e_void; ++i) {
    for (int j = 0; float_types[j] != Scalar::e_void; ++j) {
      Scalar lhs(2);
      EXPECT_TRUE(lhs.Promote(float_types[i])) << "float promotion #" << i;
      Scalar rhs(0.5f);
      EXPECT_TRUE(rhs.Promote(float_types[j])) << "float promotion #" << j;
      Scalar x(2.5f);
      EXPECT_TRUE(x.Promote(float_types[j]));
      EXPECT_EQ(lhs + rhs, x);
    }
  }
}

TEST(ScalarTest, SetValueFromCString) {
  Scalar a;

  EXPECT_THAT_ERROR(
      a.SetValueFromCString("1234567890123", lldb::eEncodingUint, 8).ToError(),
      Succeeded());
  EXPECT_EQ(1234567890123ull, a);

  EXPECT_THAT_ERROR(
      a.SetValueFromCString("-1234567890123", lldb::eEncodingSint, 8).ToError(),
      Succeeded());
  EXPECT_EQ(-1234567890123ll, a);

  EXPECT_THAT_ERROR(
      a.SetValueFromCString("asdf", lldb::eEncodingSint, 8).ToError(),
      Failed());
  EXPECT_THAT_ERROR(
      a.SetValueFromCString("asdf", lldb::eEncodingUint, 8).ToError(),
      Failed());
  EXPECT_THAT_ERROR(
      a.SetValueFromCString("1234567890123", lldb::eEncodingUint, 4).ToError(),
      Failed());
  EXPECT_THAT_ERROR(a.SetValueFromCString("123456789012345678901234567890",
                                          lldb::eEncodingUint, 8)
                        .ToError(),
                    Failed());
  EXPECT_THAT_ERROR(
      a.SetValueFromCString("-123", lldb::eEncodingUint, 8).ToError(),
      Failed());
}
