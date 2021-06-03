#include "FISHJSON.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*����ڴ�й¶*/
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
	/*c����ʵ�ֻ�����Ͷ�ջ,�ο�RapidJSON��*/
	char* stack;
	unsigned int size, top;//������Ҫ��̬��չ��top��ʹ��ָ�����ͣ�����push��pop��������ͨ���򵥵�+-��ʵ��
}fish_context;

/*ѹ���ջ������ֵΪ����ͷָ��λ��*/
static void* fish_context_push(fish_context* c, unsigned int size) {
	void* ret;
	assert(size > 0);
	if (c->top + size >= c->size) {
		if (c->size == 0)
			c->size = FISH_PARSE_STACK_INIT_SIZE;
		while (c->top + size >= c->size)/*�ο�C++ STL vector  �Ż� 2����չ��1.5����չ*/
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
/*������ջ*/
static void* fish_context_pop(fish_context* c, unsigned int size) {
	assert(c->top >= size);
	return c->stack + (c->top -= size);
}
/*�����հ׷�*/
static void fish_parse_whitespace(fish_context* c) {
	char *p = c->json;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
		p++;
	c->json = p;
}

/*����TRUE*/
static int fish_parse_true(fish_context* c, fish_value* v){
	EXPECT(c, 't');//�����Ա�̣���fish_parse_value���Ѿ��жϹ��ˣ����Ƕ��ڹ��ܺ�������Ҫ���ǵ���Ӧ�ó���������test.c�ļ��еĵ�Ԫ���ԡ�
	if (c->json[0] != 'r' || c->json[1] != 'u' || c->json[2] != 'e')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 3;
	v->type = FISH_TRUE;
	return FISH_PARSE_OK;
}
/*����FALSE*/
static int fish_parse_false(fish_context* c, fish_value* v){
	EXPECT(c, 'f');
	if (c->json[0] != 'a' || c->json[1] != 'l' || c->json[2] != 's'||c->json[3] !='e')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 4;
	v->type = FISH_FALSE;
	return FISH_PARSE_OK;
}

/*���ò���ֵ*/
void fish_set_boolean(fish_value* v, int b) {
	fish_free(v);
	v->type = b ? FISH_TRUE : FISH_FALSE;
}

/*����null*/
static int fish_parse_null(fish_context* c, fish_value* v){
	EXPECT(c, 'n');
	if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')
		return FISH_PARSE_INVALID_VALUE;
	c->json += 3;
	v->type = FISH_NULL;
	return FISH_PARSE_OK;
}

/*��������*/
static int fish_parse_number(fish_context* c, fish_value* v) {
	/*����У���Ƿ����JSON�������͵��﷨Ҫ��*/
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
	/*���Ƿ�����̫����*/
	if (errno == ERANGE &&( v->n == HUGE_VAL||v->n==-HUGE_VAL))return FISH_PARSE_NUMBER_TOO_BIG;
	if (c->json == NULL)
		return FISH_PARSE_INVALID_VALUE;
	c->json = p;
	v->type = FISH_NUMBER;
	return FISH_PARSE_OK;
}

/*��������*/
void fish_set_number(fish_value* v, double n) {
	fish_free(v);
	v->n = n;
	v->type = FISH_NUMBER;
}

/*����string-��������*/
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
/*�����ַ���-���Ƶ�v��*/
static int fish_parse_string(fish_context* c, fish_value* v) {
	int ret;
	char* s;
	unsigned int len;
	if ((ret = fish_parse_string_raw(c, &s, &len)) == FISH_PARSE_OK)
		fish_set_string(v, s, len);
	return ret;
}

/*����value��s��ֵ*/
void fish_set_string(fish_value* v, const char* s, unsigned int len) {
	assert(v != NULL && (s != NULL || len == 0)&& v->s != NULL);
	fish_free(v);
	v->s = (char*)malloc(len + 1);
	memcpy(v->s, s, len);
	v->s[len] = '\0';
	v->len = len;
	v->type = FISH_STRING;
}

/*����array*/
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
			break;//����ֱ��return������free
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

/*��������*/
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
		/*����key*/
		if (*c->json != '"') {
			ret = FISH_PARSE_MISS_KEY;
			break;
		}
		if ((ret = fish_parse_string_raw(c, &str,&m.key_len)) != FISH_PARSE_OK)
			break;
		memcpy(m.key = (char*)malloc(m.key_len + 1), str, m.key_len);
		m.key[m.key_len] = '\0';
		
		/*����ð��*/
		fish_parse_whitespace(c);
		if (*c->json != ':') {
			ret = FISH_PARSE_MISS_COLON;
			break;
		}
		c->json++;
		fish_parse_whitespace(c);

		/*����value*/
		if ((ret = fish_parse_value(c, &m.v)) != FISH_PARSE_OK)
			break;
		memcpy(fish_context_push(c, sizeof(fish_member)), &m, sizeof(fish_member));
		size++;
		m.key = NULL;
		
		/*������ }*/
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

/*������ĸ�������value*/
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

/*JSON-�ı����� */
int fish_parse(fish_value* v, const char* json) {
	assert(v != NULL);
	fish_context c;
	int ret;
	c.json = json;
	/*��ʼ��*/
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
	assert(c.top == 0);//��������Ƿ�ȫ��������
	free(c.stack);
	return ret;
}

/*��������*/
static void fish_stringify_number(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_NUMBER);
	char buf[256];
	unsigned int length = snprintf(buf,sizeof(buf), "%.17g", v->n);
	PUTS(c, buf, length);
}
/*�����ַ���*/
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
/*��������*/
static void fish_stringify_array(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_ARRAY);
	PUTC(c, '[');
	for (unsigned int i = 0; i < v->arr_size; i++) {
		if (i)PUTC(c, ',');
		fish_stringify_value(c, &v->e[i]);
	}
	PUTC(c, ']');
}
/*���ɶ���*/
static void fish_stringify_object(fish_context* c, const fish_value* v) {
	assert(v != NULL&&v->type==FISH_OBJECT);
	PUTC(c, '{');
	for (unsigned int i = 0; i < v->obj_size; i++) {
		if (i)PUTC(c, ',');
		fish_stringify_string(c, v->m[i].key, v->m[i].key_len);
		PUTC(c, ':');
		fish_stringify_value(c, &v->m[i].v);
		/*6.3 bug��λ��object���ɲ��ԣ�fish_stringify_value���δ���*/
	}
	PUTC(c, '}');
}

/*����value*/
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

/*JSON-�ı�����*/
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

/*�ڴ����*/
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
	v->type = FISH_NULL;//�����ظ��ͷ� ���Ʊ��
}

/* ��ȡ�ṹ��fish_value�����Ϣ */

/*��ȡ����*/
fish_type fish_get_type(const fish_value* v) {
    assert(v != NULL);
    return v->type;
}
/*��ȡ����*/
double fish_get_number(const fish_value* v) {
	assert(v != NULL && v->type == FISH_NUMBER);
	return v->n;
}
/*��ȡboolֵ*/
int fish_get_boolean(const fish_value* v) {
	assert(v != NULL && (v->type == FISH_TRUE || v->type == FISH_FALSE));
	return v->type==FISH_TRUE;
}
/*��ȡ�ַ���*/
const char* fish_get_string(const fish_value* v) {
	assert(v != NULL && v->type == FISH_STRING);
	return v->s;
}
/*��ȡ�ַ�������*/
unsigned int fish_get_stringlen(const fish_value* v) {
	assert(v != NULL && v->type == FISH_STRING);
	return v->len;
}
/*��ȡ�����С*/
unsigned int fish_get_array_size(const fish_value* v) {
	assert(v != NULL && v->type == FISH_ARRAY);
	return v->arr_size;
}
/*��ȡ�����±�Ϊpos��Ԫ�ص�ָ��*/
fish_value* fish_get_array_element(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_ARRAY && pos<v->arr_size);
	return &v->e[pos];
}
/*��ȡ�����С*/
unsigned int fish_get_object_size(const fish_value* v) {
	assert(v != NULL && v->type == FISH_OBJECT);
	return v->obj_size;
}
/*��ȡ�����±�Ϊpos��key��ָ��*/
const char* fish_get_object_key(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return v->m[pos].key;
}
/*��ȡ�����±�Ϊpos��key�ĳ���*/
unsigned int fish_get_object_key_len(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return v->m[pos].key_len;
}
/*��ȡ�����±�Ϊpos��v��ָ��*/
fish_value* fish_get_object_value(const fish_value* v, unsigned int pos) {
	assert(v != NULL && v->type == FISH_OBJECT && pos < v->obj_size);
	return &v->m[pos].v;
}