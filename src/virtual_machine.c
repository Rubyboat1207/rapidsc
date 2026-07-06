#include "virtual_machine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "math.h"

void func_return(VmState_t* state, Frame_t* callerFrame, Frame_t *fnFrame);

void run(VmState_t* state, RapidsProgram_t *program) {
    state->globals = malloc(program->header.globalsCount * sizeof(RapidsVariable_t*));
    // todo: function closure list.
    int globalImportIndex = 0;
    for(int i = 0; i < program->header.moduleCount; i++) {
        ModuleImport_t import = program->header.modules[i];

        for(int moduleIdx = 0; moduleIdx < state->moduleRegistry->moduleCount; moduleIdx++) {
            Module_t *module = state->moduleRegistry->modules[moduleIdx];
            if(strcmp(import.moduleName, module->identifier) != 0) {
                continue;
            }

            for(int exportIdx = 0; exportIdx < module->exportCount; exportIdx++) {
                for(int importIdx = 0; importIdx < import.importCount; importIdx++) {
                    if(strcmp(module->exportNames[exportIdx], import.imports[importIdx]) != 0) {
                        continue;
                    }

                    RapidsVariable_t *global = &module->exports[exportIdx];
                    global->refCount = -1; // Has the chance to double assign -1 to these variables, but fine ultimately.
                    state->globals[globalImportIndex++] = global;
                }
            }
        }
    }


    state->frames[0].localCount = program->header.outermostLocalsCount;
    state->frames[0].locals = malloc(program->header.outermostLocalsCount * sizeof(Cell_t*));
    for(int i = 0; i < program->header.outermostLocalsCount; i++) {
        state->frames[0].locals[i] = calloc(sizeof(Cell_t), 1);
        state->frames[0].locals[i]->refCount = -1;
    }
    state->frames[0].functionIndex = -1;
    Frame_t *frame = &state->frames[state->curFrame];

    while(frame->pc < (frame->functionIndex == -1 ? program->commandCount : program->functions.functions[frame->functionIndex].commandCount)) {
        frame = &state->frames[state->curFrame];
        // printf("%d\n", frame->pc);
        Command_t command = (frame->functionIndex == -1 ?
            program->commands :
            program->functions.functions[frame->functionIndex].commands
        )[frame->pc++];

        switch(command.opcode) {
            case OP_LOAD_STRING: {
                pushFrameOwned(frame, var_str(program->header.strings[command.iVal]));
                break;
            }
            case OP_LOAD_GLOBAL: {
                pushFrame(frame, state->globals[command.iVal]);
                break;
            }
            case OP_LOAD_FUNCTION: {
                pushFrameOwned(frame, var_bytecode(command.iVal));
                break;
            }
            case OP_CAPTURE_CLOSURE: {
                RapidsVariable_t *var = frame->stack[frame->nextOpenStackPos - 1];
                int captureSize = 0;
                int currentArraySize = 2;
                Cell_t **capture = malloc(sizeof(Cell_t*) * currentArraySize);

                if(var->type != RAPIDS_VAR_TYPE_BYTECODE_FUNC) {
                    // error
                    break;
                }
                for(int j = 0; j <= state->curFrame; j++) {
                    for(int i = 0; i < state->frames[j].localCount; i++) {
                        Cell_t *local = state->frames[j].locals[i];
                        if (local->var) {
                            if(captureSize + 1 >= currentArraySize) {
                                capture = realloc(capture, sizeof(Cell_t*) * ceil(currentArraySize * 1.6));
                            }
                            capture[captureSize++] = local;
                            cellRetain(local);
                        }
                    }
                }
                var->bytecodeFuncVal.capture = realloc(capture, sizeof(Cell_t) * captureSize);
                var->bytecodeFuncVal.captureCount = captureSize;

                break;
            }
            case OP_STORE_LOCAL: {
                var_release(frame->locals[command.iVal]->var);
                frame->locals[command.iVal]->var = popFrame(frame);
                break;
            }
            case OP_LOAD_LOCAL: {
                pushFrame(frame, frame->locals[command.iVal]->var);
                break;
            }
            case OP_LOAD_BOOL: {
                pushFrameOwned(frame, var_bool(command.iVal ? true : false));
                break;
            }
            case OP_LOAD_NUMBER: {
                pushFrameOwned(frame, var_num(command.dVal));
                break;
            }
            case OP_CONCAT: {
                int offset = (sizeof(RapidsVariable_t*) * (frame->nextOpenStackPos - (command.iVal)));
                RapidsVariable_t **first = frame->stack + offset;
                char* str = vars_concat(first, command.iVal);
                for(int i = 0; i < command.iVal; i++) {
                    var_release(popFrame(frame));
                }
                pushFrameOwned(frame, var_str_transfer(str));
                break;
            }
            case OP_ASSEMBLE_LIST: {
                RapidsVariable_t **first = &frame->stack[frame->nextOpenStackPos - command.iVal];
                RapidsVariable_t *list = var_list(first, command.iVal);
                for(int i = 0; i < command.iVal; i++) {
                    var_release(popFrame(frame));
                }
                pushFrameOwned(frame, list);
                break;
            }
            case OP_JUMP: {
                frame->pc = command.iVal;
                break;
            }
            case OP_JUMP_REL: {
                frame->pc += command.iVal;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                RapidsVariable_t *var = popFrame(frame);
                if(!var_truthy(var))
                    frame->pc = command.iVal;
                var_release(var);
                break;
            }
            case OP_JUMP_IF_TRUE: {
                RapidsVariable_t *var = popFrame(frame);
                if(var_truthy(var))
                    frame->pc = command.iVal;
                var_release(var);
                break;
            }
            case OP_JUMP_IF_FALSE_REL: {
                RapidsVariable_t *var = popFrame(frame);
                if(!var_truthy(var))
                    frame->pc += command.iVal;
                var_release(var);
                break;
            }
            case OP_JUMP_IF_TRUE_REL: {
                RapidsVariable_t *var = popFrame(frame);
                if(var_truthy(var))
                    frame->pc += command.iVal;
                var_release(var);
                break;
            }
            case OP_NOT: {
                RapidsVariable_t *var = popFrame(frame);
                pushFrameOwned(frame, var_bool(!var_truthy(var)));
                var_release(var);
                break;
            }
            case OP_SUBTRACT:
            case OP_MULTIPLY:
            case OP_DIVIDE:
            case OP_OR:
            case OP_AND:
            case OP_EQUAL:
            case OP_ADD:
            {
                static RapidsVariable_t *(*const mathOps[])(RapidsVariable_t*, RapidsVariable_t*) = {
                    [OP_ADD]      = vars_add,
                    [OP_SUBTRACT] = vars_subtract,
                    [OP_MULTIPLY] = vars_multiply,
                    [OP_DIVIDE]   = vars_divide,
                    [OP_AND]      = vars_and,
                    [OP_OR]       = vars_or,
                    [OP_EQUAL]    = vars_equals
                };
                RapidsVariable_t *a = popFrame(frame);
                RapidsVariable_t *b = popFrame(frame);

                pushFrameOwned(frame, mathOps[command.opcode](a, b));
                var_release(a);
                var_release(b);
                break;
            }
            case OP_GET_ITERATOR: {
                RapidsVariable_t *var = popFrame(frame);
                pushFrameOwned(frame, var_get_iterator(var));
                var_release(var);
                break;
            }
            case OP_ITERATOR_COMPLETE: {
                RapidsVariable_t *var = popFrame(frame);
                pushFrameOwned(frame, var_bool(var_iterator_completed(var)));
                var_release(var);
                break;
            }
            case OP_ITERATOR_NEXT: {
                RapidsVariable_t *var = popFrame(frame);
                var_iterator_next(var);
                var_release(var);
                break;
            }
            case OP_PUSH_ITER_VALUE: {
                RapidsVariable_t *var = popFrame(frame);
                pushFrame(frame, var->iteratorVal.values[var->iteratorVal.idx]);
                var_release(var);
                break;
            }
            case OP_PUSH_ITER_KEY: {
                RapidsVariable_t *var = popFrame(frame);
                if(var->iteratorVal.keyAsIndex) {
                    pushFrame(frame, var_num(var->iteratorVal.idx));
                }else {
                    pushFrame(frame, var_str(var->iteratorVal.keys[var->iteratorVal.idx]));
                }

                var_release(var);
                break;
            }
            case OP_CALL: {
                RapidsVariable_t *popped = popFrame(frame);
                var_retain(popped);
                Frame_t *fnFrame;
                int params;
                int closureSize = 0;

                if(popped->type == RAPIDS_VAR_TYPE_NATIVE_FUNC) {
                    fnFrame = calloc(sizeof(Frame_t), 1);
                    fnFrame->localCount = popped->nativeFuncVal.params;
                    params = popped->nativeFuncVal.params;
                }
                else if(popped->type == RAPIDS_VAR_TYPE_BYTECODE_FUNC) {
                    fnFrame = &state->frames[++state->curFrame];
                    fnFrame->functionIndex = popped->bytecodeFuncVal.index;
                    closureSize = popped->bytecodeFuncVal.captureCount;
                    RapidsBytecodeFunction_t fn = program->functions.functions[fnFrame->functionIndex];
                    fnFrame->localCount = fn.localCount + fn.parameterCount + closureSize;
                    params = fn.parameterCount;
                    fnFrame->pc = 0;
                }else {
                    // error
                    break;
                }

                // Load parameters
                fnFrame->locals = malloc(sizeof(Cell_t*) * fnFrame->localCount);
                for(int i = 0; i < params; i++) {
                    RapidsVariable_t *var = popFrame(frame);
                    Cell_t *cell = malloc(sizeof(Cell_t));
                    cell->constant = true;
                    cell->refCount = 1; // equivalent to cellRetain
                    cell->var = var;

                    fnFrame->locals[params - 1 - i] = cell;
                    var_retain(var);
                }

                if(popped->type == RAPIDS_VAR_TYPE_NATIVE_FUNC) {
                    popped->nativeFuncVal.fn(fnFrame);
                    func_return(state, frame, fnFrame);
                }
                if(popped->type == RAPIDS_VAR_TYPE_BYTECODE_FUNC) {
                    // load closure
                    for(int i = 0; i < popped->bytecodeFuncVal.captureCount; i++) {
                        Cell_t *cell = popped->bytecodeFuncVal.capture[i];
                        fnFrame->locals[params + i] = cell;
                        cellRetain(cell);
                    }
                }
                var_release(popped);

                break;
            }
            case OP_RETURN: {
                Frame_t *fnFrame = frame;
                frame = &state->frames[--state->curFrame];
                func_return(state, frame, fnFrame);
                break;
            }
            case OP_EXIT: {
                releaseFrame(frame);

                free(state->frames[0].locals);
                free(state->globals);
                return;
            }
            default: {
                printf("found unimplemented opcode %d\n", command.opcode);
            }
        }
    }
}

void func_return(VmState_t* state, Frame_t* callerFrame, Frame_t *fnFrame) {
    RapidsVariable_t *returnedVariable = popFrame(fnFrame);
    if(returnedVariable->bVal) {
        pushFrameOwned(callerFrame, popFrame(fnFrame));
    }
    var_release(returnedVariable);
    releaseFrame(fnFrame);
    free(fnFrame->locals);
}

RapidsVariable_t *popFrame(Frame_t *frame) {
    RapidsVariable_t* var = frame->stack[--frame->nextOpenStackPos];
    return var;
}

void pushFrame(Frame_t *frame, RapidsVariable_t *variable) {
    if(variable == NULL) {
        printf("null variable pushed to the stack. Segfault incoming.\n");
    }
    frame->stack[frame->nextOpenStackPos++] = variable;
    var_retain(variable);
}

void pushFrameOwned(Frame_t *frame, RapidsVariable_t *variable) {
    frame->stack[frame->nextOpenStackPos++] = variable;
}

void register_module(ModuleRegistry_t* registry, Module_t* module) {
    registry->modules = realloc(registry->modules, sizeof(Module_t*) * ++registry->moduleCount);
    registry->modules[registry->moduleCount - 1] = module;
}

void releaseFrame(Frame_t *frame) {
    for(int i = 0; i < frame->localCount; i++) {
        cellRelease(frame->locals[i]);
    }
}

void cellRelease(Cell_t *cell) {
    if(!cell || cell->refCount == -1) return;
    if(--cell->refCount <= 0) {
        var_release(cell->var);
        free(cell);
    }
}

void cellRetain(Cell_t *cell) {
    if(!cell || cell->refCount == -1) return;
    cell->refCount++;
}
