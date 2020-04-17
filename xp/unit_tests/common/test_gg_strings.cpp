// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
  |   includes
  +---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_strings.h"

TEST_GROUP(GG_STRINGS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_STRINGS, Test_String_Create) {
    GG_String str;
    str = GG_String_Create("test string");
    STRNCMP_EQUAL("test string", GG_String_GetChars(&str), 11);

    str = GG_String_Create("");
    STRCMP_EQUAL("", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_Clone) {
    GG_String str;
    GG_String str_clone;
    str = GG_String_Create("test");
    str_clone = GG_String_Clone(&str);
    STRCMP_EQUAL(GG_String_GetChars(&str), GG_String_GetChars(&str_clone));

    str = GG_String_Create("");
    str_clone = GG_String_Clone(&str);
    STRCMP_EQUAL(GG_String_GetChars(&str), GG_String_GetChars(&str_clone));
}

TEST(GG_STRINGS, Test_String_CreateFromSubString) {
    GG_String str;
    str = GG_String_CreateFromSubString("TestString", 4, 3);
    STRCMP_EQUAL("Str", GG_String_GetChars(&str));

    str = GG_String_CreateFromSubString("TestString", 0, 10);
    STRCMP_EQUAL("TestString", GG_String_GetChars(&str));

    str = GG_String_CreateFromSubString("TestString", 0, 14);
    STRCMP_EQUAL("TestString", GG_String_GetChars(&str));

    str = GG_String_CreateFromSubString("", 0, 4);
    STRCMP_EQUAL("", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_Assign) {
    GG_Result result;
    GG_String str;
    str = GG_String_Create("test");
    result = GG_String_Assign(&str, "assigned");
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("assigned", GG_String_GetChars(&str));

    result = GG_String_Assign(&str, NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(NULL == str.chars);
}

TEST(GG_STRINGS, Test_String_AssignN) {
    GG_Result result;
    GG_String str;
    str = GG_String_Create("test");
    result = GG_String_AssignN(&str, "string", 3);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("str", GG_String_GetChars(&str));

    result = GG_String_AssignN(&str, "", 0);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(NULL == str.chars);
}

TEST(GG_STRINGS, Test_String_Reserve) {
    GG_Result result;
    GG_String str;
    str = GG_String_Create("");
    result = GG_String_Reserve(&str, 8);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(8, GG_String_GetBuffer(&str)->allocated);

    str = GG_String_Create("string");
    result = GG_String_Reserve(&str, 8);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(12, GG_String_GetBuffer(&str)->allocated);

    str = GG_String_Create("string");
    result = GG_String_Reserve(&str, 4);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(6, GG_String_GetBuffer(&str)->allocated);
}

TEST(GG_STRINGS, Test_String_Copy) {
    GG_String str1;
    GG_String str2;
    GG_String str3;
    str1 = GG_String_Create("Test");
    str2 = GG_String_Create("");
    str3 = GG_String_Create("String");
    GG_String_Copy(&str1, &str2);
    CHECK(NULL == str1.chars);
    CHECK(NULL == str2.chars);

    GG_String_Copy(&str2, &str3);
    STRCMP_EQUAL("String", GG_String_GetChars(&str2));
    STRCMP_EQUAL("String", GG_String_GetChars(&str3));

    GG_String_Copy(&str1, &str3);
    STRCMP_EQUAL("String", GG_String_GetChars(&str1));
    STRCMP_EQUAL("String", GG_String_GetChars(&str3));

    GG_String_SetLength(&str2, 0);
    GG_String_Copy(&str1, &str2);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
    STRCMP_EQUAL("", GG_String_GetChars(&str2));
}

TEST(GG_STRINGS, Test_String_SetLength) {
    GG_String str;
    GG_Result result;
    str = GG_String_Create("");
    result = GG_String_SetLength(&str, 4);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    GG_String str1;
    str1 = GG_String_Create("Test");
    result = GG_String_SetLength(&str1, 3);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, GG_String_GetBuffer(&str1)->allocated);
    CHECK_EQUAL(3, GG_String_GetBuffer(&str1)->length);

    result = GG_String_SetLength(&str1, 6);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);
}

TEST(GG_STRINGS, Test_String_Append) {
    GG_Result result;
    GG_String str;
    str = GG_String_Create("");
    result = GG_String_Append(&str, "my");
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("my", GG_String_GetChars(&str));

    result = GG_String_Append(&str, "Test");
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("myTest", GG_String_GetChars(&str));

    result = GG_String_Append(&str, "");
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("myTest", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_AppendChar) {
    GG_Result result;
    GG_String str;
    str = GG_String_Create("");
    result = GG_String_AppendChar(&str, 'm');
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("m", GG_String_GetChars(&str));

    result = GG_String_AppendChar(&str, 'T');
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("mT", GG_String_GetChars(&str));

    result = GG_String_Append(&str, NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("mT", GG_String_GetChars(&str));

    result = GG_String_Append(&str, "");
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("mT", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_Compare) {
    GG_String str;
    int result;
    str = GG_String_Create("Test !");
    result = GG_String_Compare(&str, "Test !", false);
    CHECK_EQUAL(0, result);

    result = GG_String_Compare(&str, "tesT !", true);
    CHECK_EQUAL(0, result);

    result = GG_String_Compare(&str, "tesT !", false);
    CHECK(0 > result);

    result = GG_String_Compare(&str, "Te", false);
    CHECK(0 < result);

    result = GG_String_Compare(&str, "Te", true);
    CHECK(0 < result);

    result = GG_String_Compare(&str, "Test !2", false);
    CHECK(0 > result);

    result = GG_String_Compare(&str, "", false);
    CHECK(0 < result);
}

TEST(GG_STRINGS, Test_String_Equals) {
    bool result;
    GG_String str;
    str = GG_String_Create("Test !");
    result = GG_String_Equals(&str, "Test !", false);
    CHECK_EQUAL(true, result);

    result = GG_String_Equals(&str, "tesT !", true);
    CHECK_EQUAL(true, result);

    result = GG_String_Equals(&str, "tesT !", false);
    CHECK_EQUAL(false, result);

    result = GG_String_Equals(&str, "Te", false);
    CHECK_EQUAL(false, result);

    result = GG_String_Equals(&str, "Te", true);
    CHECK_EQUAL(false, result);

    result = GG_String_Equals(&str, "Test !2", false);
    CHECK_EQUAL(false, result);

    result = GG_String_Equals(&str, "", false);
    CHECK_EQUAL(false, result);
}

TEST(GG_STRINGS, Test_String_SubString) {
    GG_String subStr;
    GG_String str;
    str = GG_String_Create("My string");
    subStr = GG_String_SubString(&str, 3, 3);
    STRCMP_EQUAL("str", GG_String_GetChars(&subStr));
}

TEST(GG_STRINGS, Test_String_StartsWith) {
    GG_String str;
    bool result;
    str = GG_String_Create("myTest");
    result = GG_String_StartsWith(&str, "my");
    CHECK_EQUAL(true, result);

    result = GG_String_StartsWith(&str, "myTest");
    CHECK_EQUAL(true, result);

    result = GG_String_StartsWith(&str, "me");
    CHECK_EQUAL(false, result);

    result = GG_String_StartsWith(&str, "myTest1");
    CHECK_EQUAL(false, result);

    result = GG_String_StartsWith(&str, "");
    CHECK_EQUAL(true, result);

    GG_String str1;
    str1 = GG_String_Create("");
    result = GG_String_StartsWith(&str1, "my");
    CHECK_EQUAL(false, result);

    GG_String empty;
    GG_String_Init(empty);
    LONGS_EQUAL(0, GG_String_GetLength(&empty));
    result = GG_String_StartsWith(&empty, "");
    CHECK_EQUAL(true, result);
}

TEST(GG_STRINGS, Test_String_EndsWith) {
    GG_String str;
    bool result;
    str = GG_String_Create("myTest");
    result = GG_String_EndsWith(&str, "Test");
    CHECK_EQUAL(true, result);

    result = GG_String_EndsWith(&str, "myTest");
    CHECK_EQUAL(true, result);

    result = GG_String_EndsWith(&str, "Te");
    CHECK_EQUAL(false, result);

    result = GG_String_EndsWith(&str, "");
    CHECK_EQUAL(true, result);

    GG_String str1;
    str1 = GG_String_Create("");
    result = GG_String_EndsWith(&str1, "my");
    CHECK_EQUAL(false, result);
}

TEST(GG_STRINGS, Test_String_FindStringFrom) {
    GG_String str;
    int result;
    str = GG_String_Create("myTest");
    result = GG_String_FindStringFrom(&str, "Test", 0);
    CHECK_EQUAL(2, result);

    result = GG_String_FindStringFrom(&str, "Test", 2);
    CHECK_EQUAL(2, result);

    result = GG_String_FindStringFrom(&str, "myTest", 0);
    CHECK_EQUAL(0, result);

    result = GG_String_FindStringFrom(&str, "myTest2", 0);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindStringFrom(&str, "myTest2", 0);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindStringFrom(&str, "Test", 3);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindStringFrom(&str, "Test", 6);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindStringFrom(&str, "", 0);
    CHECK_EQUAL(0, result);
}

TEST(GG_STRINGS, Test_String_FindString) {
    GG_String str;
    int result;
    str = GG_String_Create("myTest");
    result = GG_String_FindString(&str, "Test");
    CHECK_EQUAL(2, result);

    result = GG_String_FindString(&str, "myTe");
    CHECK_EQUAL(0, result);

    result = GG_String_FindString(&str, "myTest2");
    CHECK_EQUAL(-1, result);

    result = GG_String_FindString(&str, "re");
    CHECK_EQUAL(-1, result);

    result = GG_String_FindString(&str, "");
    CHECK_EQUAL(0, result);
}

TEST(GG_STRINGS, Test_String_FindChar) {
    GG_String str;
    int result;
    str = GG_String_Create("myTest");
    result = GG_String_FindChar(&str, 'T');
    CHECK_EQUAL(2, result);

    result = GG_String_FindChar(&str, 'S');
    CHECK_EQUAL(-1, result);

    result = GG_String_FindChar(&str, '\0');
    CHECK_EQUAL(-1, result);
}

TEST(GG_STRINGS, Test_String_FindCharFrom) {
    GG_String str;
    int result;
    str = GG_String_Create("myTest");
    result = GG_String_FindCharFrom(&str, 'T', 0);
    CHECK_EQUAL(2, result);

    result = GG_String_FindCharFrom(&str, 'T', 2);
    CHECK_EQUAL(2, result);

    result = GG_String_FindCharFrom(&str, 'T', 6);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindCharFrom(&str, 'T', 4);
    CHECK_EQUAL(-1, result);

    result = GG_String_FindChar(&str, 'S');
    CHECK_EQUAL(-1, result);
}

TEST(GG_STRINGS, Test_String_ReverseFindChar) {
    GG_String str;
    int result;
    str = GG_String_Create("1+2+34");
    result = GG_String_ReverseFindChar(&str, '+');
    CHECK_EQUAL(3, result);

    result = GG_String_ReverseFindChar(&str, '-');
    CHECK_EQUAL(-1, result);
}

TEST(GG_STRINGS, Test_String_ReverseFindCharFrom) {
    GG_String str;
    int result;
    str = GG_String_Create("1+2+34");
    result = GG_String_ReverseFindCharFrom(&str, '+', 3);
    CHECK_EQUAL(1, result);

    result = GG_String_ReverseFindCharFrom(&str, '+', 2);
    CHECK_EQUAL(3, result);

    result = GG_String_ReverseFindCharFrom(&str, '+', 7);
    CHECK_EQUAL(-1, result);

    result = GG_String_ReverseFindCharFrom(&str, '-', 3);
    CHECK_EQUAL(-1, result);
}

TEST(GG_STRINGS, Test_String_ReversedFindString) {
    GG_String str;
    int result;
    str = GG_String_Create("1+2+234");
    result = GG_String_ReverseFindString(&str, "+2");
    CHECK_EQUAL(3, result);

    result = GG_String_ReverseFindString(&str, "+345");
    CHECK_EQUAL(-1, result);

    result = GG_String_ReverseFindString(&str, "+2+");
    CHECK_EQUAL(1, result);

    result = GG_String_ReverseFindString(&str, "");
    LONGS_EQUAL(GG_String_GetLength(&str), result);
}

TEST(GG_STRINGS, Test_String_MakeLowercase) {
    GG_String str;
    str = GG_String_Create("AbcD+!M.");
    GG_String_MakeLowercase(&str);
    STRCMP_EQUAL("abcd+!m.", GG_String_GetChars(&str));

    GG_String str1;
    str1 = GG_String_Create("");
    GG_String_MakeLowercase(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
}

TEST(GG_STRINGS, Test_String_MakeUppercase) {
    GG_String str;
    str = GG_String_Create("AbcD+!M.");
    GG_String_MakeUppercase(&str);
    STRCMP_EQUAL("ABCD+!M.", GG_String_GetChars(&str));

    GG_String str1;
    str1 = GG_String_Create("");
    GG_String_MakeUppercase(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
}

TEST(GG_STRINGS, Test_String_ToLowercase) {
    GG_String str;
    GG_String result;
    str = GG_String_Create("AbcD+!M.");
    result = GG_String_ToLowercase(&str);
    STRCMP_EQUAL("AbcD+!M.", GG_String_GetChars(&str));
    STRCMP_EQUAL("abcd+!m.", GG_String_GetChars(&result));

    GG_String str1;
    GG_String result1;
    str1 = GG_String_Create("");
    result1 = GG_String_ToLowercase(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
    STRCMP_EQUAL("", GG_String_GetChars(&result1));
}

TEST(GG_STRINGS, Test_String_ToUppercase) {
    GG_String str;
    GG_String result;
    str = GG_String_Create("AbcD+!M.");
    result = GG_String_ToUppercase(&str);
    STRCMP_EQUAL("AbcD+!M.", GG_String_GetChars(&str));
    STRCMP_EQUAL("ABCD+!M.", GG_String_GetChars(&result));

    GG_String str1;
    GG_String result1;
    str1 = GG_String_Create("");
    result1 = GG_String_ToLowercase(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
    STRCMP_EQUAL("", GG_String_GetChars(&result1));
}

TEST(GG_STRINGS, Test_String_ToInteger) {
    GG_Result result;
    int val;
    GG_String str;
    str = GG_String_Create("4567");
    result = GG_String_ToInteger(&str, &val);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4567, val);

    GG_String str1;
    str1 = GG_String_Create("2.3");
    result = GG_String_ToInteger(&str1, &val);
    CHECK_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    GG_String str2;
    str2 = GG_String_Create("2147483648");
    result = GG_String_ToInteger(&str2, &val);
    CHECK_EQUAL(GG_ERROR_OVERFLOW, result);
}

TEST(GG_STRINGS, Test_String_Replace) {
    GG_String str;
    str = GG_String_Create("+ab2+cbs+");
    GG_String_Replace(&str, '+', '-');
    STRCMP_EQUAL("-ab2-cbs-", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_Insert) {
    GG_String str;
    str = GG_String_Create("my gg");
    GG_Result result;
    result = GG_String_Insert(&str, "cool ", 3);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("my cool gg", GG_String_GetChars(&str));

    result = GG_String_Insert(&str, "!", 10);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("my cool gg!", GG_String_GetChars(&str));

    result = GG_String_Insert(&str, "", 0);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("my cool gg!", GG_String_GetChars(&str));

    result = GG_String_Insert(&str, "", 5);
    CHECK_EQUAL(GG_SUCCESS, result);
    STRCMP_EQUAL("my cool gg!", GG_String_GetChars(&str));

    result = GG_String_Insert(&str, "", 100);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);
    STRCMP_EQUAL("my cool gg!", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_TrimWhiteSpaceLeft) {
    GG_String str;
    str = GG_String_Create("\r\n\t    foo bar  \r\n\t");
    GG_String_TrimWhitespaceLeft(&str);
    STRCMP_EQUAL("foo bar  \r\n\t", GG_String_GetChars(&str));

    GG_String_TrimWhitespaceLeft(&str);
    STRCMP_EQUAL("foo bar  \r\n\t", GG_String_GetChars(&str));

    GG_String str1;
    str1 = GG_String_Create("");
    GG_String_TrimWhitespaceLeft(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
}

TEST(GG_STRINGS, Test_String_TrimCharsLeft) {
    GG_String str;
    str = GG_String_Create("+++foo bar+++");
    GG_String_TrimCharLeft(&str, '+');
    STRCMP_EQUAL("foo bar+++", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_TrimWhitespaceRight) {
    GG_String str;
    str = GG_String_Create("\r\n\t    foo bar  \r\n\t");
    GG_String_TrimWhitespaceRight(&str);
    STRCMP_EQUAL("\r\n\t    foo bar", GG_String_GetChars(&str));

    GG_String_TrimWhitespaceRight(&str);
    STRCMP_EQUAL("\r\n\t    foo bar", GG_String_GetChars(&str));

    GG_String str1;
    str1 = GG_String_Create("");
    GG_String_TrimWhitespaceRight(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
}

TEST(GG_STRINGS, Test_String_TrimCharsRight) {
    GG_String str;
    str = GG_String_Create("+++foo bar+++");
    GG_String_TrimCharRight(&str, '+');
    STRCMP_EQUAL("+++foo bar", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_TrimWhitespace) {
    GG_String str;
    str = GG_String_Create("\r\n\t    foo bar  \r\n\t");
    GG_String_TrimWhitespace(&str);
    STRCMP_EQUAL("foo bar", GG_String_GetChars(&str));

    GG_String_TrimWhitespace(&str);
    STRCMP_EQUAL("foo bar", GG_String_GetChars(&str));

    GG_String str1;
    str1 = GG_String_Create("");
    GG_String_TrimWhitespace(&str1);
    STRCMP_EQUAL("", GG_String_GetChars(&str1));
}

TEST(GG_STRINGS, Test_String_TrimChar) {
    GG_String str;
    str = GG_String_Create("+++foo bar+++");
    GG_String_TrimChar(&str, '+');
    STRCMP_EQUAL("foo bar", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_TrimChars) {
    GG_String str;
    str = GG_String_Create("++--+-foo bar+++---");
    GG_String_TrimChars(&str, "+-");
    STRCMP_EQUAL("foo bar", GG_String_GetChars(&str));
}

TEST(GG_STRINGS, Test_String_Add) {
    GG_String str;
    GG_String result;
    str = GG_String_Create("foo");
    result = GG_String_Add(&str, "bar");
    STRCMP_EQUAL("foobar", GG_String_GetChars(&result));

    GG_String str1;
    GG_String result1;
    str1 = GG_String_Create("");
    result1 = GG_String_Add(&str1, "foo");
    STRCMP_EQUAL("foo", GG_String_GetChars(&result1));

    GG_String str2;
    GG_String result2;
    str2 = GG_String_Create("foo");
    result2 = GG_String_Add(&str2, "");
    STRCMP_EQUAL("foo", GG_String_GetChars(&result2));
}
