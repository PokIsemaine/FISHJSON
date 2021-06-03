#include "FISHJSON.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*检测内存泄露*/
#ifdef _WIN32
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define PUTC(c, ch)         do { *(char*)fish_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c,s,len)		do { memcpy(fish_context_push(c, len), s, len);} while(0)
#ifndef FISH_PARSE_STACK_INIT_SIZE
#define FISH_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef FISH_PARSE_STRINGIFY_INIT_SIZE
#define FISH_PARSE_STRINGIFY_INIT_SIZE 256
#endif

typedef struct {
	const char* json;
	/*c语言实现混合类型堆栈,参考RapidJSON库*/
	char* stack;
	unsigned int size, top;//由于需要动态扩展，top不使用指针类型，并且push和pop操作可以通过简单的+-来实现
}fish_context;

/*压入堆栈，返回值为数据头指针位置*/
static void* fish_context_push(fish_context* c, unsigned int size) {
	void* ret;
	assert(size > 0);
	if (c->top + size >= c->size) {
		if (c->size == 0)
			c->size = FISH_PARSE_STACK_INIT_SIZE;
		while (c->top + size >= c->size)/*参考C++ STL vector  优化 2倍扩展变1.5倍扩展*/
			c->size += c->size >> 1;
		void* new_ptr= (char*)realloc(c->stack, c->size);
		if (new_ptr == NULL) {
			free(new_ptr);
			puts("FISH_CONTEXT_PUSH_CAN_NOT_REALLOC!");
		}
		else {
			c->stack = new_ptr;
		}
	}
	ret = c->stack + c->top;
	c->top += size;
	return ret;
}

static void fish_stringify_value(fish_context* c, const fish_value* v);
/*弹出堆栈*/
static void* fish_context_pop(fish_context* c, unsigned int size) {
	assert(c->top >= size);
	return c->stack + (c->top -= size);
}
/*解析空白符*/
static void fish_parse_whitespace(fish_context* c) {
	char *p = c->json;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	c->json = p;
}

/*解析TRUE*/
static int fish_parse_true(fish_context* c, fish_value* v){
	EXPECT(c, 't');//防御性编程，在fish_parse_value中已经判断过了，但是对于功能函数还需要考虑单独应用场景，例如test.c文件中的单元测试。
	if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 3;
	v->type = FISH_TRUE;
	return FISH_PARSE_OK;
}
/*解析FALSE*/
static int fish_parse_false(fish_context* c, fish_value* v){
	EXPECT(c, 'f');
	if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's'||c->json[3] !='e')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 4;
	v->type = FISH_FALSE;
	return FISH_PARSE_OK;
}

/*设置布尔值*/
void fish_set_boolean(fish_value* v, int b) {
	fish_free(v);
	v->type = b ? FISH_TRUE : FISH_FALSE;
}

/*解析null*/
static int fish_parse_null(fish_context* c, fish_value* v){
	EXPECT(c, 'n');
	if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 3;
	v->type = FISH_NULL;
	return FISH_PARSE_OK;
}

/*解析数字*/
static int fish_parse_number(fish_context* c, fish_value* v) {
	/*数字校验是否符合JSON数字类型的语法要求*/
	char* p = c->json;
	if (*p == '-')p++;
	if (*p == '0')p++;
	else {
		if (!isdigit(*p)||*p=='0')return FISH_PARSE_INVALID_VALUE;
		for (p++; isdigit(*p); p++);
	}
	if (*p == '.') {
		p++;
		if (!isdigit(*p))return FISH_PARSE_INVALID_VALUE;
		for (p++; isdigit(*p); p++);
	}
	if (*p == 'e' || *p == 'E') {
		p++;
		if (*p == '+' || *p == '-')p++;
		if (!isdigit(*p))return FISH_PARSE_INVALID_VALUE;
		for (p++; isdigit(*p); p++);
	}
	errno = 0;

	v->n = strtod(c->json, NULL);
	/*判是否数字太大了*/
	if (errno == ERANGE &&( v->n == HUGE_VAL||v->n==-HUGE_VAL))return FISH_PARSE_NUMBER_TOO_BIG;
	if (c->json == NULL)
		return FISH_PARSE_INVALID_VALUE;
	c->json = p;
	v->type = FISH_NUMBER;
	return FISH_PARSE_OK;
}

/*设置数字*/
void fish_set_number(fish_value* v, double n) {
	fish_free(v);
	v->n = n;
	v->type = FISH_NUMBER;
}

/*解析string-解析部分*/
static int fish_parse_string_raw(fish_context* c, char** str, unsigned int* len) {
	unsigned int head = c->top;
	const char* p;
	EXPECT(c, '\"');
	p = c->json;
	for (;;) {
		char ch = *p++;
		if (ch == '\"')
		{
			*len = c->top - head;
			*str = fish_context_pop(c, *len);
			c->json = p;
			return FISH_PARSE_OK;
		}
		else if (ch == '\\')
		{
			switch (*p++) {
			case '\"':PUTC(c, '\"'); break;
			case '\\':PUTC(c, '\\'); break;
			case '/':PUTC(c, '/'); break;
			case 'b':PUTC(c, '\b'); break;
			case 'f':PUTC(c, '\f'); break;
			case 'n':PUTC(c, '\n'); break;
			case 'r':PUTC(c, '\r'); break;
			case 't':PUTC(c, '\t'); break;
			default:
				c->top = head;
				return FISH_PARSE_INVALID_STRING_ESCAPE;
			}
		}
		else if (ch == '\0')
		{
			c->top = head;
			return FISH_PARSE_MISS_QUOTATION_MARK;
		}
		else
		{
			if ((unsigned char)ch < 0x20) {
				c->top = head;
				return FISH_PARSE_INVALID_STRING_CHAR;
			}
			PUTC(c, ch);
		}
	}
}
/*解析字符串-复制到v中*/
static int fish_parse_string(fish_context* c, fish_value* v) {
	int ret;
	char* s;
	unsigned int len;
	if ((ret = fish_parse_string_raw(c, &s, &len)) == FISH_PARSE_OK)
		fish_set_string(v, s, len);
	return ret;
}

/*设置value中s的值*/
void fish_set_string(fish_value* v, const char* s, unsigned int len) {
	assert(v != NULL && (s != NULL || len == 0)&& v->s != NULL);
	fish_free(v);
	v->s = (char*)malloc(len + 1);
	memcpy(v->s, s, len);
	v->s[len] = '\0';
	v->len = len;
	v->type = FISH_STRING;
}

/*解析array*/
static int fish_parse_array(fish_context* c, fish_value* v) {
	EXPECT(c,'[');
	unsigned int size = 0;
	int ret;
	fish_parse_whitespace(c);
	if (*c->json == ']') {
		c->json++;
		v->type = FISH_ARRAY;
		v->arr_size = 0;
		v->e = NULL;
		return FISH_PARSE_OK;
	}
	for (;;) {
		fish_value e;
		fish_init(&e);
		if ((ret = fish_parse_value(c, &e)) != FISH_PARSE_OK)
			break;//报错不直接return，下面free
		memcpy(fish_context_push(c, sizeof(fish_value)), &e, sizeof(fish_value));
		size++;
		fish_parse_whitespace(c);
		char ch = *c->json;
		if (ch == ','){
			c->json++;
			fish_parse_whitespace(c);
		}
		else if (ch == ']') {
			c->json++;
			v->type = FISH_ARRAY;
			v->arr_size = size;
			size *= sizeof(fish_value);
			memcpy(v->e = (fish_value*)malloc(size), fish_context_pop(c, size), size);
			return FISH_PARSE_OK;
		}
		else{
			ret = FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
			break;
		}
	}	
	for (unsigned int i = 0; i < size; i++)
		fish_free((fish_value*)fish_context_pop(c, sizeof(fish_value)));
	return ret;
}

/*解析对象*/
static int fish_parse_object(fish_context* c, fish_value* v) {
	EXPECT(c, '{');
	fish_member m;
	unsigned int size;
	int ret;
	fish_parse_whitespace(c);
	if (*c->json == '}') {
		c->json++;
		v->type = FISH_OBJECT;
		v->m = 0;
		v->obj_size = 0;
		return FISH_PARSE_OK;
	}
	m.key = NULL;
	size = 0;
	for (;;) {
		fish_init(&m.v);
		char* str;
		/*解析key*/
		if (*c->json != '"') {
			ret = FISH_PARSE_MISS_KEY;
			break;
		}
		if ((ret = fish_parse_string_raw(c, &str,&m.key_len)) != FISH_PARSE_OK)
			break;
		memcpy(m.key = (char*)malloc(m.key_len + 1), str, m.key_len);
		m.key[m.key_len] = '\0';
		
		/*解析冒号*/
		fish_parse_whitespace(c);
		if (*c->json != ':') {
			ret = FISH_PARSE_MISS_COLON;
			break;
		}
		c->json++;
		fish_parse_whitespace(c);

		/*解析value*/
		if ((ret = fish_parse_value(c, &m.v)) != FISH_PARSE_OK)
			break;
		memcpy(fish_context_push(c, sizeof(fish_member)), &m, sizeof(fish_member));
		size++;
		m.key = NULL;
		
		/*解析， }*/
		fish_parse_whitespace(c);
		if (*c->json == ',') {
			c->json++;
			fish_parse_whitespace(c);
		}
		else if (*c->json == '}') {
			unsigned int s = sizeof(fish_member) * size;
			c->json++;
			v -> type = FISH_OBJECT;
			v->obj_size = size;
			memcpy(v->m = (fish_member*)malloc(s), fish_context_pop(c, s), s);
			return FISH_PARSE_OK;
		}
		else {
			ret = FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
			break;
		}
	}
	free(m.key);
	for (unsigned int i = 0; i < size; i++) {
		fish_member* m = (fish_member*)fish_context_pop(c, sizeof(fish_member));
		free(m->key);
		fish_free(&m->v);
	}
	v->type = FISH_NULL;
	return ret;
}

/*按首字母分类解析value*/
static int fish_parse_value(fish_context* c, fish_value* v) {
	switch (*c->json) {
	case 't':	return fish_parse_true(c, v);
	case 'f':	return fish_parse_false(c, v);
	case 'n':	return fish_parse_null(c, v);
	case '\0':	return FISH_PARSE_EXPECT_VALUE;
	case '"':	return fish_parse_string(c, v);
	case '[':	return fish_parse_array(c, v);
	case '{':	return fish_parse_object(c, v);
	default:	return fish_parse_number(c, v);
	}
}

/*JSON-文本解析 */
int fish_parse(fish_value* v, const char* json) {
	assert(v != NULL);
	fish_context c;
	int ret;
	c.json = json;
	/*初始化*/
	c.stack = NULL;
	c.size = c.top = 0;
	fish_init(v);

	fish_parse_whitespace(&c);
	if ((ret = fish_parse_value(&c, v)) == FISH_PARSE_OK) {
		fish_parse_whitespace(&c);
		if (*c.json != '\0') {
			v->type = FISH_NULL;
			ret = FISH_PARSE_ROOT_NOT_SINGULAR;
		}
	}
	assert(c.top == 0);//检查数据是否全部弹出了
	free(c.stack);
	return ret;
}

/*生成数字*/
static void fish_stringify_number(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_NUMBER);
	char buf[256];
	unsigned int length = snprintf(buf,sizeof(buf), "%.17g", v->n);
	PUTS(c, buf, length);
}
/*生成字符串*/
static void fish_stringify_string(fish_context* c, const char* s, unsigned int len) {
	assert(s != NULL);
	PUTC(c, '"');
	for (unsigned int i = 0; i < len; i++) {
		unsigned char ch = s[i];
		switch (ch)
		{
			case '\"': PUTS(c, "\\\"", 2); break;
			case '\\': PUTS(c, "\\\\", 2); break;
			case '\b': PUTS(c, "\\b", 2); break;
			case '\f': PUTS(c, "\\f", 2); break;
			case '\n': PUTS(c, "\\n", 2); break;
			case '\r': PUTS(c, "\\r", 2); break;
			case '\t': PUTS(c, "\\t", 2); break;
			default:   PUTC(c, s[i]);
		}
	}
	PUTC(c, '"');
}
/*生成数组*/
static void fish_stringify_array(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_ARRAY);
	PUTC(c, '[');
	for (unsigned int i = 0; i < v->arr_size; i++) {
		if (i)PUTC(c, ',');
		fish_stringify_value(c, &v->e[i]);
	}
	PUTC(c, ']');
}
/*生成对象*/
static void fish_stringify_object(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_OBJECT);
	PUTC(c, '{');
	for (unsigned int i = 0; i < v->obj_size; i++) {
		if (i)PUTC(c, ',');
		fish_stringify_string(c, v->m[i].key, v->m[i].key_len);
		PUTC(c, ':');
		fish_stringify_value(c, &v->m[i].v);
		/*6.3 bug定位，object生成测试，fish_stringify_value传参错误*/
	}
	PUTC(c, '}');
}

/*生成value*/
static void fish_stringify_value(fish_context* c, const fish_value* v) {
	assert(v != NULL);
	switch (v->type) {
	case FISH_NULL:		PUTS(c, "null", 4); break;
	case FISH_TRUE:		PUTS(c, "true", 4); break;
	case FISH_FALSE:	PUTS(c, "false", 5); break;
	case FISH_NUMBER:	fish_stringify_number(c, v); break;
	case FISH_STRING:	fish_stringify_string(c, v->s, v->len); break;
	case FISH_ARRAY:	fish_stringify_array(c, v); break;
	case FISH_OBJECT:	fish_stringify_object(c, v); break;
	default:			assert(0 && "invalid type"); 
	}
}

/*JSON-文本生成*/
char* fish_stringify(const fish_value* v, unsigned int* length) {
	assert(v != NULL);
	fish_context c;
	c.stack = (char*)malloc(c.size = FISH_PARSE_STRINGIFY_INIT_SIZE);
	c.top = 0;
	fish_stringify_value(&c, v);
	if (length)*length = c.top;
	PUTC(&c, '\0');
	return c.stack;
}

/*内存管理*/
void fish_free(fish_value* v){
	assert(v != NULL);
	switch (v->type) {
		case FISH_STRING:	
			free(v->s);
			break;
		case FISH_ARRAY:
			for (unsigned int i = 0; i < v->arr_size; i++)
				fish_free(&v->e[i]);
			free(v->e);
			break;
		case FISH_OBJECT:
			for (unsigned int i = 0; i < v->obj_size; i++) {
				free(v->m[i].key);
				fish_free(&v->m[i].v);
			}
			free(v->m);
			break;
		default: break;
	}
	v->type = FISH_NULL;//避免重复释放 狗牌标记
}

/* 获取结构体fish_value里的信息 */

/*获取类型*/
fish_type fish_get_type(const fish_value* v) {
    assert(v != NULL);
    return v->type;
}
/*获取数字*/
double fish_get_number(const fish_value* v) {
	assert(v != NULL && v->type == FISH_NUMBER);
	return v->n;
}
/*获取bool值*/
int fish_get_boolean(const fish_value* v) {
	assert(v != NULL && (v->type == FISH_TRUE || v->type == FISH_FALSE));
	return v->type==FISH_TRUE;
}
/*获取字符串*/
const char* fish_get_string(const fish_value* v) {
	assert(v != NULL && v->type == FISH_STRING);
	return v->s;
}
/*获取字符串长度*/
unsigned int fish_get_stringlen(const fish_value* v) {
	assert(v != NULL && v->type == FISH_STRING);
	return v->len;
}
/*获取数组大小*/
unsigned int fish_get_array_size(const fish_value* v) {
	assert(v != NULL && v->type == FISH_ARRAY);
	return v->arr_size;
}
/*获取数组下标为pos的元素的指针*/
fish_value* fish_get_array_element(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_ARRAY && pos<v->arr_size);
	return &v->e[pos];
}
/*获取对象大小*/
unsigned int fish_get_object_size(const fish_value* v) {
	assert(v != NULL && v->type == FISH_OBJECT);
	return v->obj_size;
}
/*获取对象下标为pos的key的指针*/
const char* fish_get_object_key(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return v->m[pos].key;
}
/*获取对象下标为pos的key的长度*/
unsigned int fish_get_object_key_len(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return v->m[pos].key_len;
}
/*获取对象下标为pos的v的指针*/
fish_value* fish_get_object_value(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return &v->m[pos].v;
}