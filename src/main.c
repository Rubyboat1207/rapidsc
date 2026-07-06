#include<stdio.h>
#include<stdlib.h>
#include "rapids_bytecode_parser.h"
#include "virtual_machine.h"
#include "parser/lexer.h"

void print(Frame_t *frame) {
    char *str = var_as_string(frame->locals[0]->var);
    printf("%s\n", str);
    free(str);

    pushFrameOwned(frame, var_bool(false));
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        printf("Please input a file path as your first argument.\n");
        return 1;
    }

    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long fileLength = ftell(f);
    rewind(f);

    // 1gb
    if(fileLength > 1000000000) {
        printf("Binary too large. No clue how you managed to create a 1gb+ binary, but I congratulate you.\n");
        return 1;
    }

    uint8_t *buffer = malloc(fileLength * sizeof(uint8_t));
    fread(buffer, 1, fileLength, f);
    fclose(f);

    if(!has_valid_header(buffer, fileLength)) {
        // try compiling it down.
        auto lexRes = lex((char*) buffer);

        printf("token count: %d\n", lexRes->tokens->len);

        for(int i = 0; i < lexRes->tokens->len; i++) {
            print_token(lexRes->tokens->items[i]);
        }

        return 0;
    }

    RapidsProgram_t *program = rapids_program_parse(buffer, fileLength);

    ModuleRegistry_t *registry = calloc(sizeof(ModuleRegistry_t), 1);

    Module_t *console = malloc(sizeof(Module_t));

    console->identifier = "console";
    console->exportNames = (const char*[]){"print"};
    console->exports = (RapidsVariable_t*){var_native(print, 1)};
    console->exportCount = 1;

    register_module(registry, console);

    VmState_t *state = calloc(sizeof(VmState_t), 1);
    state->moduleRegistry = registry;
    run(state, program);
}
