#include "list.h"
#include "math.h"
#include <stdlib.h>
#include <string.h>

void listAppend(List_t* list, void* item) {
    if(list->len + 1 > list->_allocLen) {
        list->_allocLen = (int) ceil(list->_allocLen * 1.6);
        list->items = realloc(list->items, sizeof(void*) * list->_allocLen);
        memset(list->items + list->len, 0, list->_allocLen - list->len);
    }
    list->items[list->len++] = item;
}

void listRemove(List_t *list, void *item) {
    for(int i = 0; i < list->len; i++) {
        if(list->items[i] != item) {
            continue;
        }

        listRemoveAt(list, i);
        return;
    }
}

void listRemoveAt(List_t *list, int idx) {
    if(idx < 0 || idx >= list->len) {
        return;
    }
    if(idx != list->len - 1) {
        int itemsAfter = list->len - idx - 1;
        memmove(&list->items[idx], &list->items[idx + 1], sizeof(void*) * itemsAfter);
    }

    list->len -= 1;
}

void listForeach(List_t *list, void (*loop)(void *, int)) {
    for(int i = 0; i < list->len; i++) {
        loop(list->items[i], i);
    }
}

List_t *listNew() {
    List_t* new = malloc(sizeof(List_t));

    new->_allocLen = 1;
    new->len = 0;
    new->items = calloc(sizeof(void*), 1);

    return new;
}

void listFree(List_t *list) {
    free(list->items);
    free(list);
}

void listAppendAll(List_t* list, List_t* other) {
    for(int i = 0; i < other->len; i++) {
        listAppend(list, other->items[i]);
    }
}
