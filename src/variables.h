#include "stdbool.h"
#include "utils/list.h"

#ifndef VARIABLES
#define VARIABLES

typedef struct Frame_t Frame_t;

typedef struct Cell_t Cell_t;
typedef struct RapidsVariable_t RapidsVariable_t;

typedef enum {
    RAPIDS_VAR_TYPE_NUMBER=1,
    RAPIDS_VAR_TYPE_STRING = 2,
    RAPIDS_VAR_TYPE_BOOL = 3,
    RAPIDS_VAR_TYPE_LIST = 4,
    RAPIDS_VAR_TYPE_ITERATOR = 5,
    RAPIDS_VAR_TYPE_BYTECODE_FUNC = 6,
    RAPIDS_VAR_TYPE_NATIVE_FUNC = 7,
    RAPIDS_VAR_TYPE_NULL = 8
} RapidsVariableType;

typedef struct {
    void(*fn)(Frame_t*);
    int params;
} NativeFunction_t;

typedef struct {
    int index;
    Cell_t **capture;
    int captureCount;
} BytecodeFunction_t;

typedef struct {
    bool keyAsIndex;
    char** keys;
    RapidsVariable_t **values;
    int len;
    int idx;
} Iterator_t;

struct RapidsVariable_t{
    int refCount;
    RapidsVariableType type;
    union {
        double dVal;
        char* sVal;
        bool bVal;
        List_t* lVal;
        BytecodeFunction_t bytecodeFuncVal;
        NativeFunction_t nativeFuncVal;
        Iterator_t iteratorVal;
    };
};

RapidsVariable_t *var_bool(bool val);
RapidsVariable_t *var_native(void(*fn)(Frame_t*), int params);
RapidsVariable_t *var_str(char* str);
RapidsVariable_t *var_list(RapidsVariable_t **variables, int len);
RapidsVariable_t *var_str_transfer(char* str);
RapidsVariable_t *var_num(double val);
RapidsVariable_t *var_bytecode(int idx);
RapidsVariable_t *var_null();
char *var_list_join(RapidsVariable_t *list, char* seperator);
RapidsVariable_t *var_get_iterator(RapidsVariable_t* var);
void var_iterator_next(RapidsVariable_t *iterator);
bool var_iterator_completed(RapidsVariable_t *iterator);
bool var_truthy(RapidsVariable_t* var);
char *var_as_string(RapidsVariable_t* var);
char *vars_concat(RapidsVariable_t** buf, int len);
RapidsVariable_t *var_get_iterator(RapidsVariable_t* var);
RapidsVariable_t *vars_add(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_subtract(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_multiply(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_divide(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_or(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_and(RapidsVariable_t* a, RapidsVariable_t* b);
RapidsVariable_t *vars_equals(RapidsVariable_t* a, RapidsVariable_t* b);
void var_retain(RapidsVariable_t *v);
void var_release(RapidsVariable_t *v);

#endif
