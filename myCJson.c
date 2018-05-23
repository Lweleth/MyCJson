/** @Date    : 2018-04-13-14.58
  * @Author  : Lweleth (SoungEarlf@gmail.com)
  * @Link    : https://github.com/
  * @Version :
  */

#ifndef MYCJSON
#define MYCJSON

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


#include "myCJson.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <stdbool.h>

#define JSON_REPORT_PERROR(msg, p) fprintf(stderr, "JSON PARSE ERROR (%d): CALL(%s) " msg " at %s\n", __LINE__, __func__, p)
#define JSON_REPORT_NERROR(msg) fprintf(stderr, "JSON PARSE ERROR (%d): CALL(%s) " msg "\n", __LINE__, __func__)
#define JSON_EXPECT(p, cond, msg) {                          \
    if(! (cond)) {                                           \
            JSON_REPORT_PERROR(msg, p);                      \
            return 0;                                        \
    };                                                       \
};

#define IS_SPECCHAR(c) ((unsigned char)(c) <= (unsigned char)' ')

static void* (*json_malloc)(size_t sz) = malloc;
static void* (*json_calloc)(size_t n, size_t sz) = calloc;
static void  (*JSON_FREE)(void *p) = free;

//misc
static inline int hex_num(char c) {

    switch(c){
        case '0' ... '9': return c - '0';
        case 'a' ... 'f': return c - 'a' + 10;
        case 'A' ... 'F': return c - 'A' + 10;
        default: JSON_REPORT_PERROR("INCORRECT UNICODE", &c); return -1;
    }
    return 0;
}

static inline json_ui_t hex_trans(const char *p){
    json_ui_t tmp = hex_num(p[0])<0?1:0 +
                    hex_num(p[1])<0?1:0 +
                    hex_num(p[2])<0?1:0 +
                    hex_num(p[3])<0?1:0 ;
    if(tmp > 0)
        return -1;
    return (hex_num(p[0]) << 12) +
           (hex_num(p[1]) << 8)  +
           (hex_num(p[2]) << 4)  +
           (hex_num(p[3]));
}

static inline char *skip_spec(char *pos) {
    while(pos && *pos && IS_SPECCHAR(*pos) ) pos++;
    return pos;
}
//tool
void json_free(myJson* json) {
    json_value_t *p = json->child, *nxt;
    while(p != NULL) {
        nxt = p->next;
        json_free(p);
        p = nxt;
    }
    JSON_FREE(json);
}

static myJson* create_json(json_type_t type, char* key, myJson* pre) {
    myJson* json = json_calloc(1, sizeof(json_value_t));
    assert(json);
    json->type = type;
    json->key = key;
    if(pre->cur == NULL) {
        pre->child = json;
        pre->cur = json;
    }
    else {
        pre->cur->next = json;
        pre->cur = json;
    }
    pre->len++;
    return json;
}

static int json_write_utf8(json_ui_t val, char*p, char* *endp) {
    if(val < 0x80) {
        *p++ = 0x7f & val;
    }
    else if(val < 0x800) {
        *p++ = 0xc0 | ((val >> 6) & 0x1f);
        *p++ = 0x80 | (val & 0x3f);
    }
    else if(val < 0x10000) {
        *p++ = 0xe0 | ((val >> 12) & 0x0f);
        *p++ = 0x80 | ((val >> 6) & 0x3f);
        *p++ = 0x80 | (val & 0x3f);
    }
    else if(val < 0x200000) {
        *p++ = 0xf0 | ((val >> 18) & 0x07);
        *p++ = 0x80 | ((val >> 12) & 0x3f);
        *p++ = 0x80 | ((val >> 6) & 0x3f);
        *p++ = 0x80 | (val & 0x3f);
    }
    else if(val < 0x4000000) {
        *p++ = 0xf8 | ((val >> 24) & 0x03);
        *p++ = 0x80 | ((val >> 18) & 0x3f);
        *p++ = 0x80 | ((val >> 12) & 0x3f);
        *p++ = 0x80 | ((val >> 6) & 0x3f);
        *p++ = 0x80 | (val & 0x3f);
    }
    else if(val < 0x80000000) {
        *p++ = 0xfc | ((val >> 30) & 0x01);
        *p++ = 0x80 | ((val >> 24) & 0x3f);
        *p++ = 0x80 | ((val >> 18) & 0x3f);
        *p++ = 0x80 | ((val >> 12) & 0x3f);
        *p++ = 0x80 | ((val >> 6) & 0x3f);
        *p++ = 0x80 | (val & 0x3f);
    }
    else {
        JSON_REPORT_PERROR("INVALID UCS", p);
        //JSON_EXPECT(NULL, false, "INVALID UCS");
        return 0;
    }
    *endp = p;
    return 1;
}

extern json_uni2_utf8 encoder = json_write_utf8;

//core
static char* unescp_string(char* str, char* *end, json_uni2_utf8 enc) {
    char *pos = str;
    char *tmp = str;
    char ch;
    while( (ch = *pos++) ) {
        if( ch == '"' ) { //close quote
            *tmp = '\0';
            *end = pos;
            return str;
        }
        else if( ch == '\\' ) {//ESCP
            switch (*pos) {
                case '\\': *tmp++ = '\\'; break;
                case '/': *tmp++ = '/'; break;
                case '"': *tmp++ = '\"'; break;
                case 'b': *tmp++ = '\b'; break;
                case 'f': *tmp++ = '\f'; break;
                case 'n': *tmp++ = '\n'; break;
                case 'r': *tmp++ = '\r'; break;
                case 't': *tmp++ = '\t'; break;
                case 'u': {
                    if ( enc == NULL) {//ensure encoder
                        JSON_REPORT_NERROR("INVALID ENCODER");
                        *tmp++ = ch;
                        break;
                    }
                    json_ui_t val = hex_trans(pos + 1);
                    //utf-16 to utf-8
                    if(val >= 0xd800 && val <= 0xdfff) {
                        //ensure "\"
                        //ensure "u"
                        JSON_EXPECT(str, *(pos + 5) == '\\', "INVALID UCS");
                        JSON_EXPECT(str, *(pos + 6) == 'u', "INVALID UCS");
                        json_ui_t low = 0x03ff & (hex_trans(pos + 7) - 0xdc00);
                        json_ui_t high= (/*0x03ff &*/ ((val - 0xd800) << 10));
                        val = 0x10000 + (low | high);
                        pos += 6;
                    }
                    if(!enc(val, tmp, &tmp)) {
                       //ensure
                       //JSON_REPORT_PERROR("INVALID CODEPOINT", tmp);
                       return 0;
                    }
                    pos += 4;
                } break;
                default: JSON_REPORT_PERROR("Unexpected char", pos - 1); *tmp++ = *pos; return 0;
            }
            pos++;
        }
        else { //normal character
            *tmp++ = ch;
        }
    }
    //report  " " " lost close quote
    JSON_REPORT_PERROR("No closing quote", str);
    return 0;
}

static int parse_comment(char* *str) {
    if (**str == '/') { //line
        char *tmp = NULL;
        if ( (tmp = strchr(*str, '\n')) == NULL ) {
            JSON_REPORT_PERROR("ENDLESS COMMENT", *str - 1);
            return 0;
        }
        else *str = tmp + 1;
    }
    else if ( **str == '*' ) { // block
        char* tmp = *str;
        do {
            tmp++;
            if(*tmp == '\0') {
                JSON_REPORT_PERROR("ENDLESS COMMENT", *str - 1);
                return 0;
            }
        } while( *tmp != '*' || *(tmp + 1) != '/' );
        *str = tmp + 2;
        return 1;
    }
    else {  // other char
        JSON_REPORT_PERROR("Unexpected char", *str - 1);
        return 0;
    }
}

//修改传入的key指针的地址
static char* parse_key(char* *key, char* str, json_uni2_utf8 enc) {
    char *pos = str;
    char ch;
    while( (ch = *pos++)!='\0' ) {
        //printf("%c", ch);
        if ( ch == '"' ) { // left quote
            //ensure encoder?
            *key = unescp_string(pos, &pos, enc);
            if( *key == NULL ) return 0; // faild return NULL ptr
            pos = skip_spec(pos);
            if( *pos == ':' ) return pos + 1; //key parse compeleted.
            //no ':' return NULL ptr
            //report
            JSON_REPORT_PERROR("Not find \':\'", pos);
            return 0;
        }
        else if ( IS_SPECCHAR(ch) || ch == ',') {// judge spec char and ','??
            //skip
            //printf("%c", ch);
        }
        else if ( ch == '}' ) { //in parse occurpy child struct
            return pos - 1;
        }
        else if ( ch == '/' ) { // comment
            //...
            parse_comment(&pos);
        }
        else { // no left quote
            //report
            JSON_REPORT_PERROR("Not find \'\"\'", pos - 1);
            return 0;
        }
    }
    //report
    JSON_REPORT_PERROR("Unexpected chars", pos - 1);
    return 0;

}

static char* parse_value(myJson* json, char* key, char* str, json_uni2_utf8 enc);

//解析对象内容 要分解析key和解析value两步
//返回解析后的位置//修改为返回状态值，直接修改指针为解析终止位置
static char* parse_object(myJson* json, char* str, json_uni2_utf8 enc) {
    while( true ) {
        char* nw_key;
        str = parse_key(&nw_key, str, enc);
        if( str == NULL ) {
            return 0;//error
        }
        if( *str == '}' ) {
            str++;
            return str;
        }
        str = parse_value(json, nw_key, str, enc);
        if( str == NULL ) {
            return 0;//error
        }
    }
}

static char* parse_array(myJson* json, char* str, json_uni2_utf8 enc) {
    while( true ) {
        str = parse_value(json, NULL, str, enc);
        if( str == NULL ) return 0;
        if( *str == ']' ) return str + 1;
    }
}
//
static char* parse_value(myJson* json, char* key, char* str, json_uni2_utf8 enc) {
    json_value_t* child;
    char *pos = str;
    char ch;
    while( 1 ) {
        switch( ch = *pos++ ) {
            case '\0':
                //report
                JSON_REPORT_PERROR("Unexpected END of text", pos - 1);
                return 0;
            case ' ': case '\t': case '\r': case '\n': case ',':
                break;
            case '{': // child
                child = create_json(JSON_OBJECT, key, json);
                return pos = parse_object(child, pos, enc);
            case '[':
                child = create_json(JSON_ARRAY, key, json);
                return pos = parse_array(child, pos, enc);
            case ']'://
                return pos - 1;
            case '"':
                child = create_json(JSON_STRING, key, json);
                child->value_text = unescp_string(pos, &pos, enc);
                if( child->value_text == NULL ) return 0;
                return pos;
            case '-': case '0' ... '9': {//使用库函数解析
                child = create_json(JSON_INTEGER, key, json);
                char* dot;
                child->value_int = strtoll(pos - 1, &dot, 0);//0 默认自动检测进制?
                child->value_double = child->value_int;
                if(dot == pos - 1 || errno == ERANGE) {
                    JSON_REPORT_PERROR("INVALID NUMBER", pos - 1);
                    return 0;
                }
                if(*dot == '.' || *dot == 'e' || *dot == 'E') {//浮点
                    child->type = JSON_DOUBLE;
                    child->value_double = strtod(pos - 1, &dot);
                    if( dot == pos - 1 || errno == ERANGE) {
                        JSON_REPORT_PERROR("INVALID NUMBER", pos - 1);
                        return 0;
                    }
                }
                return dot;
            }
            case 't':
                if( *(pos) == 'r' && *(pos + 1) == 'u' && *(pos + 2) == 'e' ) {
                    child = create_json(JSON_BOOLEAN, key, json);
                    child->value_int = 1;
                    return pos + 3;
                }
                //report
                JSON_REPORT_PERROR("INVALID VALUE", pos - 1);
                return 0;
            case 'f':
                if( *(pos) == 'a' && *(pos + 1) == 'l' && *(pos + 2) == 's' && *(pos + 3) == 'e' ) {
                    child = create_json(JSON_BOOLEAN, key, json);
                    child->value_int = 0;
                    return pos + 4;
                }
                JSON_REPORT_PERROR("INVALID VALUE", pos - 1);
                return 0;
            case 'n':
                if( *(pos) == 'u' && *(pos + 1) == 'l' && *(pos + 2) == 'l' ) {
                    child = create_json(JSON_NULL, key, json);
                    return pos + 3;
                }
                JSON_REPORT_PERROR("INVALID VALUE", pos - 1);
                return 0;
            case '/': // comment
                parse_comment(&pos);
                break;
            default:
                JSON_REPORT_PERROR("INVALID VALUE", pos - 1);
                return 0;
        }
    }
}


const myJson* json_parse(char* text) {
    myJson json = {0};
    json.type = JSON_OBJECT;
    //初始就是OBJECT结构 没有KEY值传0
    if( parse_value(&json, NULL, text, encoder) == NULL ) { //error
        if( json.child != NULL)
            json_free(json.child);
        return 0;
    }
    return json.child;
}

const myJson* json_get_object(const myJson* json, const char* key) {
    if(!json || !key) {
        //report null
        JSON_REPORT_NERROR("VOID KEY or JSON");
        return 0;
    }
    myJson* tmp = json->child;
    while ( tmp ) {
        if ( !strcmp(key, tmp->key) ) {
            return tmp;
        }
        tmp = tmp->next;
    }
    //report no find
    //
    JSON_REPORT_NERROR("NO FIND OBJECT");
    return 0;
}

const myJson* json_get_item(const myJson* json, int idx) {
    if( !json ) {
        JSON_REPORT_NERROR("VOID JSON");
        return 0;
    }
    myJson* tmp = json->child;
    while ( tmp ) {
        if( idx == 0 )
            return tmp;
        idx--;
        tmp = tmp->next;
    }
    //report out range
    JSON_REPORT_NERROR("NO FIND ITEM");
    return 0;
}

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
