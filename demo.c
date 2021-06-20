#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FISHJSON.h"
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

int main() {
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(95);
#endif

    /*文件读取演示*/
    printf("DEMO 1 读取和解析\n\n");
    fish_value v;
    fish_init(&v);
    FILE* fp1, * fp2;
    errno_t error;
    error = fopen_s(&fp1, "C:\\Users\\ZSL\\Desktop\\a.txt", "r");
    if (error != 0) {
        printf("打开文件失败\n");
    }
    else {
        int ret=fish_parse_file(&v, fp1);
        if (ret)puts("解析失败 解析错误码%d", ret);
        else puts("解析成功");
        char* json2;
        unsigned int* length;
        json2 = fish_stringify(&v, &length);//生成器演示
        printf("解析文本为%s\n", json2);
    }

    /*获取值*/
    printf("\n\nDEMO 2 获取值\n\n");
    fish_type type_v = fish_get_type(&v);
    if (type_v == FISH_NULL)        puts("数据类型NULL");
    else if (type_v == FISH_TRUE)   puts("数据类型TRUE");
    else if (type_v == FISH_FALSE)  puts("数据类型FALSE");
    else if (type_v == FISH_ARRAY)  puts("数据类型ARRAY");
    else if (type_v == FISH_OBJECT) {
        puts("数据类型OBJECT");
        unsigned int obj_siz = fish_get_object_size(&v);
        printf("objectsize=%d\n", obj_siz);
        for (int i = 0; i < obj_siz; i++) {
            printf("member[%d].key=%s\n", i, fish_get_object_key(&v, i));
            printf("member[%d].keylen=%d\n", i, fish_get_object_key_len(&v, i));
            fish_value* obj_v = fish_get_object_value(&v, i);
            fish_type obj_v_type = fish_get_type(obj_v);
            if (obj_v_type == FISH_NULL)        puts("数据类型NULL");
            else if (obj_v_type == FISH_TRUE)   puts("数据类型TRUE");
            else if (obj_v_type == FISH_FALSE)  puts("数据类型FALSE");
            else if (obj_v_type == FISH_ARRAY)  puts("数据类型ARRAY");
            else if (obj_v_type == FISH_OBJECT) puts("数据类型OBJECT");
            else if (obj_v_type == FISH_NUMBER) {
                puts("数据类型NUMBER");
                printf("NUMBER=%.g\n", fish_get_number(obj_v));
            }
            else {
                puts("数据类型STRING");
                printf("STRING=%s\n", fish_get_string(obj_v));
            }
        }
    }
    else if (type_v == FISH_NUMBER)  puts("数据类型NUMBER");
    else puts("数据类型STRING");

    /*设置值演示*/
    printf("\n\nDEMO 3 设置值演示\n\n");
    unsigned int obj_siz = fish_get_object_size(&v);
    for (int i = 0; i < obj_siz; i++) {
        fish_value* obj_v = fish_get_object_value(&v, i);
        fish_type obj_v_type = fish_get_type(obj_v);
        if (obj_v_type == FISH_NULL)        puts("数据类型NULL");
        else if (obj_v_type == FISH_TRUE) {
            printf("[TRUE]对象的第%d个成员值设置为FALSE\n", i);
            fish_set_boolean(obj_v, FISH_FALSE);
        }
        else if (obj_v_type == FISH_FALSE) {
            printf("[FALSE]对象的第%d个成员值设置为TRUE\n", i);
            fish_set_boolean(obj_v, FISH_TRUE);
        }
        else if (obj_v_type == FISH_NUMBER) {
            printf("[NUMBER]对象的第%d个成员值设置为3.1415\n", i);
            fish_set_number(obj_v, 3.1415);
        }
        else if(obj_v_type==FISH_STRING){
            printf("[STRING]对象的第%d个成员值设置为helloword\n", i);
            char str[100];
            strcpy_s(str, 100, "helloword");
            fish_set_string(obj_v, str, strlen(str));
        }
    }

    printf("\n\n设置修改后的数据\n\n");
    for (int i = 0; i < obj_siz; i++) {
        printf("member[%d].key=%s\n", i, fish_get_object_key(&v, i));
        printf("member[%d].keylen=%d\n", i, fish_get_object_key_len(&v, i));
        fish_value* obj_v = fish_get_object_value(&v, i);
        fish_type obj_v_type = fish_get_type(obj_v);
        if (obj_v_type == FISH_NULL)        puts("数据类型NULL");
        else if (obj_v_type == FISH_TRUE)   puts("数据类型TRUE");
        else if (obj_v_type == FISH_FALSE)  puts("数据类型FALSE");
        else if (obj_v_type == FISH_ARRAY)  puts("数据类型ARRAY");
        else if (obj_v_type == FISH_OBJECT) puts("数据类型OBJECT");
        else if (obj_v_type == FISH_NUMBER) {
            puts("数据类型NUMBER");
            printf("NUMBER=%.g\n", fish_get_number(obj_v));
        }
        else {
            puts("数据类型STRING");
            printf("STRING=%s\n", fish_get_string(obj_v));
        }
    }

    /*文件写入演示*/
    printf("\n\nDEMO 4 文件写入\n\n");
    error = fopen_s(&fp2, "C:\\Users\\ZSL\\Desktop\\b.txt", "w");
    if (error != 0) {
        printf("打开文件失败\n");
    }
    else {
        fish_stringify_file(&v, fp2);//文件生成器
        puts("json数据成功写入指定文件");
    }
    return 0;
}
