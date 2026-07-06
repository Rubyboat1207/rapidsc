#include "variables.h"
#include "utils/list.h"
#include "virtual_machine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



RapidsVariable_t *var_new() {
    RapidsVariable_t* var = calloc(sizeof(RapidsVariable_t), 1);

    var->refCount = 1;

    return var;
}

RapidsVariable_t *var_bool(bool val) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_BOOL;
    var->bVal = val;

    return var;
}

RapidsVariable_t *var_native(void(*fn)(Frame_t*), int params) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_NATIVE_FUNC;
    var->nativeFuncVal = (NativeFunction_t){fn, params};

    return var;
}

RapidsVariable_t *var_str(char* str) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_STRING;
    var->sVal = strdup(str);

    return var;
}

RapidsVariable_t *var_str_transfer(char* str) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_STRING;
    var->sVal = strdup(str);

    return var;
}

RapidsVariable_t *var_num(double val) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_NUMBER;
    var->dVal = val;

    return var;
}

RapidsVariable_t *var_null() {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_NULL;

    return var;
}

RapidsVariable_t *var_bytecode(int idx) {
    RapidsVariable_t* var = var_new();

    var->type = RAPIDS_VAR_TYPE_BYTECODE_FUNC;
    var->bytecodeFuncVal.index = idx;

    return var;
}

void var_release(RapidsVariable_t *v) {
    if(!v || v->refCount == -1) return;
    if(--v->refCount <= 0) {
        // printf("variable at %p freed.\n", v);
        if(v->type == RAPIDS_VAR_TYPE_STRING) {
            free(v->sVal);
        }
        if(v->type == RAPIDS_VAR_TYPE_BYTECODE_FUNC) {
            for(int i = 0; i < v->bytecodeFuncVal.captureCount; i++) {
                cellRelease(v->bytecodeFuncVal.capture[i]);
            }
        }
        if(v->type == RAPIDS_VAR_TYPE_LIST) {
            for(int i = 0; i < v->lVal->len; i++) {
                var_release(v->lVal->items[i]);
            }
            listFree(v->lVal);
        }
        if(v->type == RAPIDS_VAR_TYPE_ITERATOR) {
            for(int i = 0; i < v->iteratorVal.len; i++) {
                var_release(v->iteratorVal.values[i]);
                if(v->iteratorVal.keys)
                    free(v->iteratorVal.keys[i]);
            }
        }
        free(v);
    }
}

RapidsVariable_t *var_list(RapidsVariable_t **variables, int len) {
    RapidsVariable_t* var = var_new();
    List_t *list = listNew();

    var->type = RAPIDS_VAR_TYPE_LIST;
    for(int i = 0; i < len; i++) {
        listAppend(list, variables[i]);
        var_retain(variables[i]);
    }

    var->lVal = list;
    return var;
}

bool var_truthy(RapidsVariable_t* var) {
    switch(var->type) {
    case RAPIDS_VAR_TYPE_NUMBER: return var->dVal != 0;
    case RAPIDS_VAR_TYPE_STRING: return strlen(var->sVal) > 0;
    case RAPIDS_VAR_TYPE_BOOL: return var->bVal;
    case RAPIDS_VAR_TYPE_LIST: return var->lVal->len > 0;
    case RAPIDS_VAR_TYPE_BYTECODE_FUNC: return true;
    case RAPIDS_VAR_TYPE_NATIVE_FUNC: return true;
    case RAPIDS_VAR_TYPE_NULL: return false;
    case RAPIDS_VAR_TYPE_ITERATOR: return var_iterator_completed(var);
      break;
    }
}

void var_retain(RapidsVariable_t *v) {if(v && v->refCount != -1) v->refCount++;}

char *var_as_string(RapidsVariable_t *var) {
    switch(var->type) {
        case RAPIDS_VAR_TYPE_NUMBER: {
            char str[25];
            sprintf(str, "%g", var->dVal);
            return strdup(str);
        }
        case RAPIDS_VAR_TYPE_STRING: return strdup(var->sVal);
        case RAPIDS_VAR_TYPE_BOOL: return strdup(var->bVal ? "true" : "false");

        case RAPIDS_VAR_TYPE_BYTECODE_FUNC:
        case RAPIDS_VAR_TYPE_NATIVE_FUNC: return strdup("[func]");

        case RAPIDS_VAR_TYPE_NULL: return strdup("null");
        case RAPIDS_VAR_TYPE_LIST: return var_list_join(var, ", ");
        case RAPIDS_VAR_TYPE_ITERATOR: return NULL;
          break;
        }
}

char *vars_concat(RapidsVariable_t **variables, int count) {
    char **strings = malloc(sizeof(char *) * count);
    int totalSize = 0;

    for (int i = 0; i < count; i++) {
        strings[i] = var_as_string(variables[i]);
        totalSize += strlen(strings[i]);
    }

    char *buff = malloc(totalSize + 1);
    char *ptr = buff;

    for (int i = 0; i < count; i++) {
        size_t len = strlen(strings[i]);
        memcpy(ptr, strings[i], len);
        ptr += len;
        free(strings[i]);
    }

    *ptr = '\0';
    free(strings);
    return buff;
}

RapidsVariable_t *vars_add(RapidsVariable_t* a, RapidsVariable_t* b) {
    if(a->type == RAPIDS_VAR_TYPE_NUMBER && b->type == RAPIDS_VAR_TYPE_NUMBER) {
        return var_num(a->dVal + b->dVal);
    }

    char *strA = var_as_string(a);
    char *strB = var_as_string(b);
    int aLen = strlen(strA);
    int bLen = strlen(strB);
    int size = sizeof(char) * (aLen + bLen + 1);

    char *buf = malloc(size);
    memcpy(buf, strA, aLen);
    memcpy(buf + sizeof(char) * aLen, strB, bLen);
    buf[size] = '\0';
    free(strA);
    free(strB);

    return var_str_transfer(buf);
}


RapidsVariable_t *vars_subtract(RapidsVariable_t* a, RapidsVariable_t* b) {
    if(a->type == RAPIDS_VAR_TYPE_NUMBER && b->type == RAPIDS_VAR_TYPE_NUMBER) {
        return var_num(a->dVal - b->dVal);
    }

    return var_null();
}

RapidsVariable_t *vars_multiply(RapidsVariable_t* a, RapidsVariable_t* b) {
    if(a->type == RAPIDS_VAR_TYPE_NUMBER && b->type == RAPIDS_VAR_TYPE_NUMBER) {
        return var_num(a->dVal * b->dVal);
    }

    return var_null();
}

RapidsVariable_t *vars_divide(RapidsVariable_t* a, RapidsVariable_t* b) {
    if(a->type == RAPIDS_VAR_TYPE_NUMBER && b->type == RAPIDS_VAR_TYPE_NUMBER) {
        return var_num(a->dVal / b->dVal);
    }

    return var_null();
}

RapidsVariable_t *vars_or(RapidsVariable_t* a, RapidsVariable_t* b) {
    return var_bool(var_truthy(a) || var_truthy(b));
}

RapidsVariable_t *vars_and(RapidsVariable_t* a, RapidsVariable_t* b) {
    return var_bool(var_truthy(a) && var_truthy(b));
}

RapidsVariable_t *vars_equals(RapidsVariable_t* a, RapidsVariable_t* b) {
    if (a->type != b->type) {
        return var_bool(false);
    }

    switch (a->type) {
        case RAPIDS_VAR_TYPE_NUMBER:
            return var_bool(a->dVal == b->dVal);

        case RAPIDS_VAR_TYPE_STRING:
            return var_bool(strcmp(a->sVal, b->sVal) == 0);

        case RAPIDS_VAR_TYPE_BOOL:
            return var_bool(a->bVal == b->bVal);

        case RAPIDS_VAR_TYPE_LIST:
            return var_bool(a->lVal == b->lVal);

        case RAPIDS_VAR_TYPE_ITERATOR:
            return var_bool(a == b);

        case RAPIDS_VAR_TYPE_BYTECODE_FUNC:
            return var_bool(a->bytecodeFuncVal.index == b->bytecodeFuncVal.index &&
                             a->bytecodeFuncVal.capture == b->bytecodeFuncVal.capture);

        case RAPIDS_VAR_TYPE_NATIVE_FUNC:
            return var_bool(a->nativeFuncVal.fn == b->nativeFuncVal.fn);

        case RAPIDS_VAR_TYPE_NULL:
            return var_bool(true);

        default:
            return var_bool(false);
    }
}

char *var_list_join(RapidsVariable_t *list, char* seperator) {
    int seperatorLength = strlen(seperator);

    int stringsCount = list->lVal->len * 2 - 1;

    char **strings = malloc(sizeof(char *) * stringsCount);
    int totalSize = 0;

    for (int i = 0; i < list->lVal->len; i++) {
        strings[i*2] = var_as_string(list->lVal->items[i]);
        if(i != list->lVal->len - 1) {
            strings[abs(i*2+1)] = strdup(seperator);
            totalSize += seperatorLength;
        }

        totalSize += strlen(strings[i*2]);
    }

    char *buff = malloc(totalSize + 1);
    char *ptr = buff;

    for (int i = 0; i < stringsCount; i++) {
        size_t len = strlen(strings[i]);
        memcpy(ptr, strings[i], len);
        ptr += len;
        free(strings[i]);
    }

    *ptr = '\0';
    free(strings);
    return buff;
}

RapidsVariable_t *var_get_iterator(RapidsVariable_t* var) {
    switch(var->type) {
        case RAPIDS_VAR_TYPE_STRING: {
            RapidsVariable_t* iterator = var_new();
            int len = strlen(var->sVal);
            RapidsVariable_t** values = malloc(sizeof(RapidsVariable_t*) * len);
            for(int i = 0; i < len; i++) {
                char* ch = malloc(2 * sizeof(char));
                ch[0] = var->sVal[i];
                ch[1] = '\0';
                values[i] = var_str_transfer(ch);
            }

            iterator->type = RAPIDS_VAR_TYPE_ITERATOR;
            iterator->iteratorVal.values = values;
            iterator->iteratorVal.len = len;
            iterator->iteratorVal.keyAsIndex = true;
        }
        case RAPIDS_VAR_TYPE_LIST: {
            RapidsVariable_t* iterator = var_new();
            RapidsVariable_t** values = malloc(sizeof(RapidsVariable_t*) * var->lVal->len);

            for(int i = 0; i < var->lVal->len; i++) {
                values[i] = var->lVal->items[i];
                var_retain(values[i]);
            }

            iterator->type = RAPIDS_VAR_TYPE_ITERATOR;
            iterator->iteratorVal.values = values;
            iterator->iteratorVal.len = var->lVal->len;
            iterator->iteratorVal.keyAsIndex = true;

            return iterator;
        }

        // No iterator exists for these guys.
        case RAPIDS_VAR_TYPE_BOOL:
        case RAPIDS_VAR_TYPE_ITERATOR:
        case RAPIDS_VAR_TYPE_BYTECODE_FUNC:
        case RAPIDS_VAR_TYPE_NATIVE_FUNC:
        case RAPIDS_VAR_TYPE_NULL:
        case RAPIDS_VAR_TYPE_NUMBER:
        return NULL;
    }
}

bool var_iterator_completed(RapidsVariable_t *iterator) {
    return iterator->iteratorVal.idx >= iterator->iteratorVal.len;
}

void var_iterator_next(RapidsVariable_t *iterator) {
    iterator->iteratorVal.idx++;
}
