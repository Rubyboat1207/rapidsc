#include<stdint.h>

#ifndef PARSER
#define PARSER


typedef enum {
    OP_ADD               = 1,
    OP_SUBTRACT          = 2,
    OP_MULTIPLY          = 3,
    OP_DIVIDE            = 4,
    OP_MODULO            = 5,
    OP_INDEX             = 6,
    OP_GREATER_THAN      = 7,
    OP_LESS_THAN         = 8,
    OP_GTE               = 9,
    OP_LTE               = 10,
    OP_EQUAL             = 11,
    OP_NOT               = 12,
    OP_TRUTHY            = 13,
    OP_MEMBER_ACCESS     = 14,
    OP_JUMP              = 15,   /* + int32  */
    OP_JUMP_IF_TRUE      = 16,   /* + int32  */
    OP_JUMP_IF_FALSE     = 17,   /* + int32  */
    OP_RETURN            = 18,
    OP_CALL              = 19,
    OP_LOAD_LOCAL        = 20,   /* + int32  */
    OP_STORE_LOCAL       = 21,   /* + int32  */
    OP_LOAD_GLOBAL       = 22,   /* + int32  */
    OP_LOAD_STRING       = 23,   /* + int32  */
    OP_CONCAT            = 24,   /* + int32  */
    OP_LOAD_NUMBER       = 25,   /* + double */
    OP_CAPTURE_CLOSURE   = 26,   /* + int32  */
    OP_PUSH_FRAME        = 27,
    OP_POP_FRAME         = 28,
    OP_LOAD_BOOL         = 29,   /* + byte   */
    OP_LOAD_FUNCTION     = 30,   /* + int32  */
    OP_JUMP_REL          = 31,   /* + int32  */
    OP_JUMP_IF_TRUE_REL  = 32,   /* + int32  */
    OP_JUMP_IF_FALSE_REL = 33,   /* + int32  */
    OP_ASSEMBLE_LIST     = 34,   /* + int32  */
    OP_GET_ITERATOR      = 35,
    OP_ITERATOR_NEXT     = 36,
    OP_PUSH_ITER_KEY     = 37,
    OP_PUSH_ITER_VALUE   = 38,
    OP_GET_MEMBER        = 39,   /* + int32  */
    OP_AND               = 40,
    OP_OR                = 41,
    OP_ITERATOR_COMPLETE = 42,
    OP_EXIT              = 255,
} OpCodeId;

typedef struct {
    int opcode;
    union {
        int    iVal;
        double dVal;
    };
} Command_t;

typedef struct {
    char  *moduleName;
    int    moduleNameLength;
    char **imports;
    int    importCount;
    int   *importLengths;
} ModuleImport_t;

typedef struct {
    int             version;
    ModuleImport_t *modules;
    int             moduleCount;
    char          **strings;
    int             stringCount;
    uint32_t        globalsCount;
    uint32_t        outermostLocalsCount;
} BytecodeHeader_t;

typedef struct {
    Command_t *commands;
    int        commandCount;
    int        parameterCount;
    int        localCount;
} RapidsBytecodeFunction_t;

typedef struct {
    RapidsBytecodeFunction_t *functions;
    int                       functionCount;
} RapidsProgramFunctionBlock_t;

typedef struct {
    BytecodeHeader_t             header;
    Command_t                   *commands;
    int                          commandCount;
    RapidsProgramFunctionBlock_t functions;
} RapidsProgram_t;


RapidsProgram_t *rapids_program_parse(const uint8_t *data, long size);
void rapids_program_free(RapidsProgram_t *prog);
int has_valid_header(const uint8_t *data, long size);

#endif
