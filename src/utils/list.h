#ifndef UTILS_LIST
#define UTILS_LIST

typedef struct {
    void** items;
    int len;
    int _allocLen;
} List_t;

void listAppend(List_t* list, void* item);
void listRemove(List_t* list, void* item);
void listAppendAll(List_t* list, List_t* other);
void listRemoveAt(List_t *list, int idx);
void listForeach(List_t* list, void(loop)(void* item, int i));
List_t* listNew();
void listFree(List_t* list);

#endif
