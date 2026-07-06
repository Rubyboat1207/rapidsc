#include<stdbool.h>
#include "rapids_bytecode_parser.h"
#include "variables.h"

#ifndef VM
#define VM

#define FRAME_STACK_SIZE 1024

struct Cell_t {
    RapidsVariable_t *var;
    bool constant;
    int refCount;
};

struct Frame_t {
    Cell_t** locals;
    int localCount;
    RapidsVariable_t *stack[FRAME_STACK_SIZE];
    int nextOpenStackPos;
    int pc;
    int functionIndex;
};

RapidsVariable_t *popFrame(Frame_t *frame);
void pushFrame(Frame_t *frame, RapidsVariable_t *variable);
void pushFrameOwned(Frame_t *frame, RapidsVariable_t *variable);
void cellRetain(Cell_t *cell);
void cellRelease(Cell_t *cell);

void releaseFrame(Frame_t *frame);

// currently only supports function exports
typedef struct {
    const char* identifier;
    const char** exportNames;
    RapidsVariable_t* exports;
    int exportCount;
} Module_t;

typedef struct {
    int moduleCount;
    Module_t** modules;
} ModuleRegistry_t;

typedef struct {
     Frame_t frames[1024];
     int curFrame;
     RapidsVariable_t **globals;
     ModuleRegistry_t* moduleRegistry;
} VmState_t;

void run(VmState_t* state, RapidsProgram_t *program);
void register_module(ModuleRegistry_t* registry, Module_t* module);

#endif
