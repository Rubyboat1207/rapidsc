#include "list.h"

#ifndef UTILS_DICT
#define DICT

typedef struct {
    List_t *keys;
    List_t *items;
    int len;
} Dict_t;

void dictPut(Dict_t* dict, char* key, void* item);
void dictRemove(Dict_t dict, char* key);

#endif
