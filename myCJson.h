#ifndef MYCJSON_H_INCLUDED
#define MYCJSON_H_INCLUDED

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef long long json_ll_t;
typedef int json_int_t;
typedef unsigned long json_ul_t;
typedef unsigned int json_ui_t;
typedef double json_double_t;
typedef enum {
    JSON_NULL,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_DOUBLE,
    JSON_BOOLEAN,
    JSON_INTEGER,
    JSON_STRING,
    JSON_NONE
} json_type_t;

typedef struct json_value {
    json_type_t type;
    char*        key;
    char*        value_text;
    json_ll_t          value_int;
    json_double_t      value_double;
    json_int_t         len;
    struct json_value* child;
    struct json_value* next;
    struct json_value* cur;
} json_value_t;
typedef json_value_t myJson;

typedef struct json_settings{
    json_ul_t max_memory;
    int settings;
    // 自定义内存申请/释放函数
    void *(*json_malloc) (size_t sz);
    void (*json_free) (void *pt);
}json_settings;

typedef int (*json_uni2_utf8)(json_ui_t val, char*p, char* *endp);
extern json_uni2_utf8 decoder;
extern const myJson* json_parse(char *text);
extern const myJson* json_get_object(const myJson* json, const char* key);
extern const myJson* json_get_item(const myJson* json, int idx);
extern       void    json_free(myJson* json);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // MYCJSON_H_INCLUDED
