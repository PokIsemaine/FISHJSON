#ifndef FISHJSON_H__
#define FISHJSON_H__

/*枚举类型JSON 7种数据类型*/
typedef enum { FISH_NULL, FISH_FALSE, FISH_TRUE, FISH_NUMBER, FISH_STRING, FISH_ARRAY, FISH_OBJECT }fish_type;

/*枚举解析的返回值*/
enum {
    FISH_PARSE_OK = 0,
    FISH_PARSE_EXPECT_VALUE,
    FISH_PARSE_INVALID_VALUE,
    FISH_PARSE_ROOT_NOT_SINGULAR,
    FISH_PARSE_NUMBER_TOO_BIG,
    FISH_PARSE_MISS_QUOTATION_MARK,
    FISH_PARSE_INVALID_STRING_ESCAPE,
    FISH_PARSE_INVALID_STRING_CHAR,
    FISH_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    FISH_PARSE_MISS_KEY,
    FISH_PARSE_MISS_COLON,
    FISH_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};
/*JSON数据结构为树形结构*/
typedef struct fish_value fish_value;
typedef struct fish_member fish_member;

struct fish_value{
    char* s; unsigned int len;              //string
    fish_value* e; unsigned int arr_size;   //array
    fish_member* m; unsigned int obj_size;  //object
    double n;
    fish_type type;
};
struct fish_member {
    char* key; unsigned int key_len;
    fish_value v;
};

int fish_parse(fish_value* v, const char* json);
int fish_parse_file(fish_value* v,const FILE* fp);
char* fish_stringify(const fish_value* v, unsigned int* length);
void fish_stringify_file(const fish_value* v,FILE* fp);

double fish_get_number(const fish_value* v);
void fish_set_number(fish_value* v, double n);


int fish_get_boolean(const fish_value* v);
void fish_set_boolean(fish_value* v, int b);

const char* fish_get_string(const fish_value* v);
unsigned int fish_get_stringlen(const fish_value* v);
void fish_set_string(fish_value* v, const char* s, unsigned int length);

unsigned fish_get_array_size(const fish_value* v);
fish_value* fish_get_array_element(const fish_value* v, unsigned pos);

unsigned int fish_get_object_size(const fish_value* v);
const char* fish_get_object_key(const fish_value*, unsigned int pos);
unsigned int fish_get_object_key_len(const fish_value* v, unsigned int pos);
fish_value* fish_get_object_value(const fish_value* v, unsigned int pos);

fish_type fish_get_type(const fish_value* v);

void fish_free(fish_value* v);

#define fish_init(v) do{(v)->type=FISH_NULL;}while(0)

#endif

