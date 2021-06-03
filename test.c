/* 单元测试框架 by Milo Yip巨佬 */
/* 相关链接：https://zhuanlan.zhihu.com/p/22460835 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FISHJSON.h"
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;
/*宏的技巧反斜杠表示该行没结束*/
#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality)\
            test_pass++;\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = 1;\
        }\
    } while(0)
/*每次使用这个宏的时候如果预期不等于实际值就会报错,使用了__LINE__会直接指出在哪一行测试错误*/
/*上面是大佬提供做单元测试的宏框架，下面是利用这些宏做的Unit Test*/

#define EXPECT_EQ_INT(expect, actual)               EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual)            EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_EQ_STRING(expect, actual, alength)   EXPECT_EQ_BASE(sizeof(expect) - 1 == alength && memcmp(expect, actual, alength) == 0, expect, actual, "%s")
#define EXPECT_EQ_TRUE(actual)                      EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_EQ_FALSE(actual)                     EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")

#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (unsigned int)expect, (unsigned int)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (unsigned int)expect, (unsigned int)actual, "%zu")
#endif

/*5.19测试解析null*/
/*因为 static 函数的意思是指，该函数只作用于编译单元中，那么没有被调用时，编译器是能发现的。*/
static void test_parse_null() {
    fish_value v;
    v.type = FISH_NULL;
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "null"));
    EXPECT_EQ_INT(FISH_NULL, fish_get_type(&v));
}
/*5.19测试解析true*/
static void test_parse_true() {
    fish_value v;
    v.type = FISH_FALSE;
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "true"));
    EXPECT_EQ_INT(FISH_TRUE, fish_get_type(&v));
}
/*5.19测试解析false*/
static void test_parse_false() {
    fish_value v;
    v.type = FISH_TRUE;
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "false"));
    EXPECT_EQ_INT(FISH_FALSE, fish_get_type(&v));
}

/*5.19测试解析数字(一般性数据)*/

#define TEST_NUMBER(expect, json)\
    do {\
        fish_value v;\
        EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, json));\
        EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, fish_get_number(&v));\
    } while(0)

static void test_parse_number() {
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(0.0, "0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1.0");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.524, "-1.524");
    TEST_NUMBER(1E5, "1E5");
    TEST_NUMBER(2e10, "2e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(5E-10, "5E-10");
    TEST_NUMBER(-2E+10, "-2E+10");
    TEST_NUMBER(3E-10, "3E-10");
    TEST_NUMBER(3.14E-5, "3.14E-5");
    TEST_NUMBER(2.71e+3, "2.71e+3");

    /*补充测试 5.30极限数据测试*/
    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
    TEST_NUMBER(4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER(2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER(2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER(1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}


/*5.30测试字符串解析*/
#define TEST_STRING(expect, json)\
    do {\
        fish_value v;\
        fish_init(&v);\
        EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, json));\
        EXPECT_EQ_INT(FISH_STRING, fish_get_type(&v));\
        EXPECT_EQ_STRING(expect, fish_get_string(&v), fish_get_stringlen(&v));\
        fish_free(&v);\
    } while(0)

static void test_parse_string() {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
}


/*5.27测试设置string*/
static void test_set_string() {
    fish_value v;
    v.type = FISH_NULL;
    fish_set_string(&v, "", 0);
    EXPECT_EQ_STRING("", fish_get_string(&v), fish_get_stringlen(&v));
    fish_set_string(&v, "Hello", 5);
    EXPECT_EQ_STRING("Hello", fish_get_string(&v), fish_get_stringlen(&v));
    fish_free(&v);
}

/*5.27测试设置number*/
static void test_set_number() {
    fish_value v;
    fish_init(&v);
    fish_set_number(&v, 2.54);
    EXPECT_EQ_DOUBLE(2.54, fish_get_number(&v));
    fish_free(&v);
}

/*5.27测试设置boolean*/
static void test_set_boolean() {
    fish_value v;
    fish_init(&v);
    fish_set_string(&v, "a", 1);
    fish_set_boolean(&v, 1);
    EXPECT_EQ_TRUE(fish_get_boolean(&v));
    fish_set_boolean(&v, 0);
    EXPECT_EQ_FALSE(fish_get_boolean(&v));
    fish_free(&v);
}

/*报错测试*/
#define TEST_ERROR(error, json)\
    do {\
        fish_value v;\
        fish_init(&v);\
        v.type = FISH_FALSE;\
        EXPECT_EQ_INT(error, fish_parse(&v, json));\
        EXPECT_EQ_INT(FISH_NULL, fish_get_type(&v));\
        fish_free(&v);\
    } while(0)

/*5.19测试报错返回值EXPECT_VALUE*/
static void test_parse_expect_value() {
    TEST_ERROR(FISH_PARSE_EXPECT_VALUE, "");
    TEST_ERROR(FISH_PARSE_EXPECT_VALUE, " ");
}

/*5.19测试非法值*/
static void test_parse_invalid_value() {
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "?");

    /* 补充测试5.30测试非法数字 */
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, ".123"); 
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "1.");   
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(FISH_PARSE_INVALID_VALUE, "nan");
}

/*5.19测试ROOT_NOT_SINGULAR*/
static void test_parse_root_not_singular() {
    TEST_ERROR(FISH_PARSE_ROOT_NOT_SINGULAR, "null x");

    /* 非法数字 */
    TEST_ERROR(FISH_PARSE_ROOT_NOT_SINGULAR, "0123"); 
    TEST_ERROR(FISH_PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(FISH_PARSE_ROOT_NOT_SINGULAR, "0x123");
}
/*5.30测试过大值*/
static void test_parse_number_too_big() {
    TEST_ERROR(FISH_PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(FISH_PARSE_NUMBER_TOO_BIG, "-1e309");
}
/*5.30测试缺少引号*/
static void test_parse_missing_quotation_mark() {
    TEST_ERROR(FISH_PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(FISH_PARSE_MISS_QUOTATION_MARK, "\"abc");
}
/*5.30测试string中非法空格*/
static void test_parse_invalid_string_escape() {
    TEST_ERROR(FISH_PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(FISH_PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(FISH_PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(FISH_PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}
/*5.30测试string中非法字符*/
static void test_parse_invalid_string_char() {
    TEST_ERROR(FISH_PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(FISH_PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}
/*6.1测试数组缺]*/
static void test_parse_miss_comma_or_square_bracket() {
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1}");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[1 2");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, "[[]");
}
/*6.1测试数组缺少key*/
static void test_parse_miss_key() {
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{1:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(FISH_PARSE_MISS_KEY, "{\"a\":1,");
}
/*6.1测试对象缺少 ：*/
static void test_parse_miss_colon() {
    TEST_ERROR(FISH_PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(FISH_PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

/*6.1测试对象缺少 } 或者 , */
static void test_parse_miss_comma_or_curly_bracket() {
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

/*5.31测试解析array*/
static void test_parse_array() {
    unsigned int i, j;
    fish_value v;

    fish_init(&v);
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "[ ]"));
    EXPECT_EQ_INT(FISH_ARRAY, fish_get_type(&v));
    EXPECT_EQ_SIZE_T(0, fish_get_array_size(&v));
    fish_free(&v);

    fish_init(&v);
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(FISH_ARRAY, fish_get_type(&v));
    EXPECT_EQ_SIZE_T(5, fish_get_array_size(&v));
    EXPECT_EQ_INT(FISH_NULL, fish_get_type(fish_get_array_element(&v, 0)));
    EXPECT_EQ_INT(FISH_FALSE, fish_get_type(fish_get_array_element(&v, 1)));
    EXPECT_EQ_INT(FISH_TRUE, fish_get_type(fish_get_array_element(&v, 2)));
    EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(fish_get_array_element(&v, 3)));
    EXPECT_EQ_INT(FISH_STRING, fish_get_type(fish_get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, fish_get_number(fish_get_array_element(&v, 3)));
    EXPECT_EQ_STRING("abc", fish_get_string(fish_get_array_element(&v, 4)), fish_get_stringlen(fish_get_array_element(&v, 4)));
    fish_free(&v);

    fish_init(&v);
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(FISH_ARRAY, fish_get_type(&v));
    EXPECT_EQ_SIZE_T(4, fish_get_array_size(&v));
    for (i = 0; i < 4; i++) {
        fish_value* a = fish_get_array_element(&v, i);
        EXPECT_EQ_INT(FISH_ARRAY, fish_get_type(a));
        EXPECT_EQ_SIZE_T(i, fish_get_array_size(a));
        for (j = 0; j < i; j++) {
            fish_value* e = fish_get_array_element(a, j);
            EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(e));
            EXPECT_EQ_DOUBLE((double)j, fish_get_number(e));
        }
    }
    fish_free(&v);
}

/*6.1测试解析object*/
static void test_parse_object() {
    fish_value v;
    unsigned int i;

    fish_init(&v);
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, " { } "));
    EXPECT_EQ_INT(FISH_OBJECT, fish_get_type(&v));
    EXPECT_EQ_SIZE_T(0, fish_get_object_size(&v));
    fish_free(&v);

    fish_init(&v);
    EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(FISH_OBJECT, fish_get_type(&v));
    EXPECT_EQ_SIZE_T(7, fish_get_object_size(&v));
    EXPECT_EQ_STRING("n", fish_get_object_key(&v, 0), fish_get_object_key_len(&v, 0));
    EXPECT_EQ_INT(FISH_NULL, fish_get_type(fish_get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", fish_get_object_key(&v, 1), fish_get_object_key_len(&v, 1));
    EXPECT_EQ_INT(FISH_FALSE, fish_get_type(fish_get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", fish_get_object_key(&v, 2), fish_get_object_key_len(&v, 2));
    EXPECT_EQ_INT(FISH_TRUE, fish_get_type(fish_get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", fish_get_object_key(&v, 3), fish_get_object_key_len(&v, 3));
    EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(fish_get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, fish_get_number(fish_get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", fish_get_object_key(&v, 4), fish_get_object_key_len(&v, 4));
    EXPECT_EQ_INT(FISH_STRING, fish_get_type(fish_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", fish_get_string(fish_get_object_value(&v, 4)), fish_get_stringlen(fish_get_object_value(&v, 4)));
    EXPECT_EQ_STRING("a", fish_get_object_key(&v, 5), fish_get_object_key_len(&v, 5));
    EXPECT_EQ_INT(FISH_ARRAY, fish_get_type(fish_get_object_value(&v, 5)));
    EXPECT_EQ_SIZE_T(3, fish_get_array_size(fish_get_object_value(&v, 5)));
    for (i = 0; i < 3; i++) {
        fish_value* e = fish_get_array_element(fish_get_object_value(&v, 5), i);
        EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(e));
        EXPECT_EQ_DOUBLE(i + 1.0, fish_get_number(e));
    }
    EXPECT_EQ_STRING("o", fish_get_object_key(&v, 6), fish_get_object_key_len(&v, 6));
    {
        fish_value* o = fish_get_object_value(&v, 6);
        EXPECT_EQ_INT(FISH_OBJECT, fish_get_type(o));
        for (i = 0; i < 3; i++) {
            fish_value* ov = fish_get_object_value(o, i);
            EXPECT_EQ_TRUE('1' + i == fish_get_object_key(o, i)[0]);
            EXPECT_EQ_SIZE_T(1, fish_get_object_key_len(o, i));
            EXPECT_EQ_INT(FISH_NUMBER, fish_get_type(ov));
            EXPECT_EQ_DOUBLE(i + 1.0, fish_get_number(ov));
        }
    }
    fish_free(&v);
}
/*6.2,6.3测试JSON生成*/
#define TEST_ROUNDTRIP(json)\
    do {\
        fish_value v;\
        char* json2;\
        unsigned int length;\
        fish_init(&v);\
        EXPECT_EQ_INT(FISH_PARSE_OK, fish_parse(&v, json));\
        json2 = fish_stringify(&v, &length);\
        EXPECT_EQ_STRING(json, json2, length);\
        fish_free(&v);\
        free(json2);\
    } while(0)

static void test_stringify_number() {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1e+20");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");

    TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
    TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_string() {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
}

static void test_stringify_array() {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_object() {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify() {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");
    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
}
/* ... */
static void test_parse() {

    /*解析测试*/
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_object();

    /*报错测试*/
    test_parse_miss_key();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_invalid_string_char();
    test_parse_invalid_string_escape();
    test_parse_missing_quotation_mark();
    test_parse_miss_comma_or_curly_bracket();
    test_parse_miss_comma_or_square_bracket();

    /*设置测试*/
    test_set_string();
    test_set_boolean();
    test_set_number();
    
    /*生成测试*/
    test_stringify();


    /* ... */
}

int main() {
    #ifdef _WIN32
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
       // _CrtSetBreakAlloc(95);
    #endif
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);
    return main_ret;
}