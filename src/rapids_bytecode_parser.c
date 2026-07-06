/*
 * rapids_parser.c
 *
 * Parses a RapidsLang bytecode binary into a RapidsProgram_t.
 * All heap memory is owned by the returned struct; call
 * rapids_program_free() when done.
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rapids_bytecode_parser.h"

/* ── opcode catalogue ───────────────────────────────────────────────────── */

/* Returns the total byte-size of one encoded instruction (opcode byte + args). */
static int opcode_size(uint8_t byte) {
    switch ((OpCodeId)byte) {
        /* no-arg opcodes: 1 byte */
        case OP_ADD:
        case OP_SUBTRACT:
        case OP_MULTIPLY:
        case OP_DIVIDE:
        case OP_MODULO:
        case OP_INDEX:
        case OP_GREATER_THAN:
        case OP_LESS_THAN:
        case OP_GTE:
        case OP_LTE:
        case OP_EQUAL:
        case OP_NOT:
        case OP_TRUTHY:
        case OP_MEMBER_ACCESS:
        case OP_RETURN:
        case OP_CALL:
        case OP_PUSH_FRAME:
        case OP_POP_FRAME:
        case OP_GET_ITERATOR:
        case OP_ITERATOR_NEXT:
        case OP_PUSH_ITER_KEY:
        case OP_PUSH_ITER_VALUE:
        case OP_AND:
        case OP_OR:
        case OP_ITERATOR_COMPLETE:
        case OP_CAPTURE_CLOSURE:
        case OP_EXIT:
            return 1;

        /* bool operand: 1 + 1 = 2 bytes */
        case OP_LOAD_BOOL:
            return 2;

        /* int32 operand: 1 + 4 = 5 bytes */
        case OP_JUMP:
        case OP_JUMP_IF_TRUE:
        case OP_JUMP_IF_FALSE:
        case OP_LOAD_LOCAL:
        case OP_STORE_LOCAL:
        case OP_LOAD_GLOBAL:
        case OP_LOAD_STRING:
        case OP_CONCAT:
        case OP_LOAD_FUNCTION:
        case OP_JUMP_REL:
        case OP_JUMP_IF_TRUE_REL:
        case OP_JUMP_IF_FALSE_REL:
        case OP_ASSEMBLE_LIST:
        case OP_GET_MEMBER:
            return 5;

        /* double operand: 1 + 8 = 9 bytes */
        case OP_LOAD_NUMBER:
            return 9;

        default:
            fprintf(stderr, "rapids_parser: unknown opcode 0x%02x\n", byte);
            return -1;
    }
}

/* ── little-endian read helpers ─────────────────────────────────────────── */

static int32_t read_i32(const uint8_t *p) {
    int32_t v;
    memcpy(&v, p, 4);
    return v;
}

static uint32_t read_u32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

static double read_f64(const uint8_t *p) {
    double v;
    memcpy(&v, p, 8);
    return v;
}

/* ── single-instruction parser ──────────────────────────────────────────── */

/*
 * Reads one Command_t from `data` at `*offset`, advances `*offset`,
 * and fills `out`.  Returns 0 on success, -1 on unknown opcode.
 */
static int parse_one_command(const uint8_t *data, long size,
                              long *offset, Command_t *out)
{
    if (*offset >= size) return -1;

    uint8_t byte = data[*offset];
    int     sz   = opcode_size(byte);
    if (sz < 0 || *offset + sz > size) return -1;

    out->opcode       = byte;
    out->iVal   = 0; /* zero out union */
    out->dVal   = 0.0;

    switch (sz) {
        case 1:  /* no argument */
            break;
        case 2:  /* byte argument (LoadBool) */
            out->iVal = data[*offset + 1];
            break;
        case 5:  /* int32 argument */
            out->iVal = read_i32(data + *offset + 1);
            break;
        case 9:  /* double argument */
            out->dVal = read_f64(data + *offset + 1);
            break;
    }

    *offset += sz;
    return 0;
}

/* ── opcode-array parser ────────────────────────────────────────────────── */

/*
 * Parse exactly `count` opcodes from `data` starting at `*offset`.
 * Allocates and returns the Command_t array (caller owns it).
 * `*offset` is advanced past the consumed bytes.
 * Returns NULL on error.
 */
static Command_t *parse_commands_by_count(const uint8_t *data, long size,
                                           long *offset,
                                           uint32_t count,
                                           int *out_count)
{
    Command_t *cmds = malloc(count * sizeof(Command_t));
    if (!cmds) return NULL;

    for (uint32_t i = 0; i < count; i++) {
        if (parse_one_command(data, size, offset, &cmds[i]) != 0) {
            free(cmds);
            return NULL;
        }
    }

    *out_count = (int)count;
    return cmds;
}

/*
 * Parse opcodes until `end_offset` is reached (used for the top-level code
 * section whose length is derived from the remaining bytes in the file).
 * Allocates and returns the Command_t array (caller owns it).
 */
static Command_t *parse_commands_to_end(const uint8_t *data, long size,
                                         long *offset, long end_offset,
                                         int *out_count)
{
    /* Upper-bound: can't have more opcodes than bytes. */
    long capacity = (end_offset - *offset);
    if (capacity <= 0) {
        *out_count = 0;
        return NULL;
    }

    Command_t *cmds = malloc((size_t)capacity * sizeof(Command_t));
    if (!cmds) return NULL;

    int n = 0;
    while (*offset < end_offset) {
        if (parse_one_command(data, size, offset, &cmds[n]) != 0) {
            free(cmds);
            return NULL;
        }
        n++;
    }

    *out_count = n;
    /* Optionally shrink to fit — useful for large files. */
    Command_t *trimmed = realloc(cmds, (size_t)n * sizeof(Command_t));
    return trimmed ? trimmed : cmds;
}

/* ── header parser ──────────────────────────────────────────────────────── */

static const uint8_t SIGNATURE[] = { 'R','P','D','<','3',' ' };
#define SIGNATURE_LEN 6

/*
 * Parses BytecodeHeader_t from `data`.
 * `*offset` must start at 0 and is advanced to the first byte after the header.
 * Returns 0 on success, -1 on error.
 */
static int parse_header(const uint8_t *data, long size,
                         long *offset, BytecodeHeader_t *out)
{
    /* Signature */
    if (size < SIGNATURE_LEN ||
        memcmp(data, SIGNATURE, SIGNATURE_LEN) != 0) {
        fprintf(stderr, "rapids_parser: bad signature\n");
        return -1;
    }
    *offset = SIGNATURE_LEN;

    /* Version (int32) */
    if (*offset + 4 > size) return -1;
    out->version = read_i32(data + *offset); *offset += 4;

    if (out->version != 0) {
        fprintf(stderr, "rapids_parser: unsupported version %d\n", out->version);
        return -1;
    }

    /*
     * totalSize: the C# code does ToInt32 but then advances offset by 8,
     * silently eating 4 extra bytes.  We replicate that behaviour exactly.
     */
    if (*offset + 8 > size) return -1;
    int32_t total_size = read_i32(data + *offset);
    *offset += 8; /* intentional: mirrors the C# offset += 8 after ToInt32 */

    if ((long)total_size > size) {
        fprintf(stderr, "rapids_parser: header declares %d bytes but file is only %ld\n",
                total_size, size);
        return -1;
    }

    /* Globals / outermost locals */
    if (*offset + 16 > size) return -1;
    out->globalsCount          = read_u32(data + *offset); *offset += 4;
    out->outermostLocalsCount  = read_u32(data + *offset); *offset += 4;

    int32_t module_count = read_i32(data + *offset); *offset += 4;
    int32_t string_count = read_i32(data + *offset); *offset += 4;

    /* Modules */
    out->moduleCount = module_count;
    out->modules     = calloc((size_t)module_count, sizeof(ModuleImport_t));
    if (!out->modules) return -1;

    for (int i = 0; i < module_count; i++) {
        if (*offset + 4 > size) return -1;
        int32_t name_len = read_i32(data + *offset); *offset += 4;

        if (*offset + name_len > size) return -1;
        out->modules[i].moduleNameLength = name_len;
        out->modules[i].moduleName       = malloc((size_t)name_len + 1);
        if (!out->modules[i].moduleName) return -1;
        memcpy(out->modules[i].moduleName, data + *offset, (size_t)name_len);
        out->modules[i].moduleName[name_len] = '\0';
        *offset += name_len;

        if (*offset + 4 > size) return -1;
        int32_t import_count = read_i32(data + *offset); *offset += 4;

        out->modules[i].importCount   = import_count;
        out->modules[i].imports       = calloc((size_t)import_count, sizeof(char *));
        out->modules[i].importLengths = calloc((size_t)import_count, sizeof(int));
        if (!out->modules[i].imports || !out->modules[i].importLengths) return -1;

        for (int j = 0; j < import_count; j++) {
            if (*offset + 4 > size) return -1;
            int32_t import_len = read_i32(data + *offset); *offset += 4;

            if (*offset + import_len > size) return -1;
            out->modules[i].importLengths[j] = import_len;
            out->modules[i].imports[j]        = malloc((size_t)import_len + 1);
            if (!out->modules[i].imports[j]) return -1;
            memcpy(out->modules[i].imports[j], data + *offset, (size_t)import_len);
            out->modules[i].imports[j][import_len] = '\0';
            *offset += import_len;
        }
    }

    /* Strings */
    out->stringCount = string_count;
    out->strings     = calloc((size_t)string_count, sizeof(char *));
    if (!out->strings) return -1;

    for (int i = 0; i < string_count; i++) {
        if (*offset + 4 > size) return -1;
        int32_t str_len = read_i32(data + *offset); *offset += 4;

        if (*offset + str_len > size) return -1;
        out->strings[i] = malloc((size_t)str_len + 1);
        if (!out->strings[i]) return -1;
        memcpy(out->strings[i], data + *offset, (size_t)str_len);
        out->strings[i][str_len] = '\0';
        *offset += str_len;
    }

    return 0;
}

/* ── function-block parser ──────────────────────────────────────────────── */

/*
 * Parses RapidsProgramFunctionBlock_t.
 * `data` points to the start of the function-block section (not the whole
 * file); `size` is the byte length of that section.
 * Returns 0 on success, -1 on error.
 */
static int parse_function_block(const uint8_t *data, long size,
                                 RapidsProgramFunctionBlock_t *out)
{
    long offset = 0;

    /*
     * The C# ToBytes() writes an 8-byte total-size field first, then a
     * uint32 function count.  FromBytes skips the first 8 bytes then reads
     * the count.
     */
    if (size < 12) return -1;
    offset += 8; /* skip the stored block size (we already know it) */

    uint32_t fn_count = read_u32(data + offset); offset += 4;

    out->functionCount = (int)fn_count;
    out->functions     = calloc(fn_count, sizeof(RapidsBytecodeFunction_t));
    if (!out->functions) return -1;

    for (uint32_t i = 0; i < fn_count; i++) {
        if (offset + 12 > size) return -1; /* need 3 × uint32 */

        /* Layout: codeCount (u32), localCount (u32), paramCount (u32), code… */
        uint32_t code_count  = read_u32(data + offset); offset += 4;
        uint32_t local_count = read_u32(data + offset); offset += 4;
        uint32_t param_count = read_u32(data + offset); offset += 4;

        out->functions[i].localCount     = (int)local_count;
        out->functions[i].parameterCount = (int)param_count;

        int cmd_count = 0;
        Command_t *cmds = parse_commands_by_count(data, size, &offset,
                                                   code_count, &cmd_count);
        if (!cmds && code_count > 0) return -1;

        out->functions[i].commands     = cmds;
        out->functions[i].commandCount = cmd_count;
    }

    return 0;
}

/* ── top-level entry point ──────────────────────────────────────────────── */

/*
 * Parses a complete RapidsLang bytecode binary.
 *
 * Parameters
 *   data  – pointer to the raw bytes
 *   size  – byte length of the buffer
 *
 * Returns a heap-allocated RapidsProgram_t on success, NULL on error.
 * Free with rapids_program_free() when done.
 */
RapidsProgram_t *rapids_program_parse(const uint8_t *data, long size)
{
    if (!data || size <= 0) return NULL;
    const uint8_t *u = (const uint8_t *)data;

    RapidsProgram_t *prog = calloc(1, sizeof(RapidsProgram_t));
    if (!prog) return NULL;

    /* ── 1. Header ── */
    long offset = 0;
    if (parse_header(u, size, &offset, &prog->header) != 0) {
        fprintf(stderr, "rapids_parser: header parse failed\n");
        free(prog);
        return NULL;
    }
    long header_end = offset;

    /* ── 2. Function block ── */
    /*
     * The block starts immediately after the header.  Its first 8 bytes
     * encode the total block size as a uint64, which we use to find where
     * the top-level code section begins.
     */
    if (header_end + 8 > size) {
        fprintf(stderr, "rapids_parser: truncated before function block\n");
        free(prog);
        return NULL;
    }

    /* Read the stored block size (written as a ulong / uint64 in C#). */
    uint64_t fn_block_size_raw;
    memcpy(&fn_block_size_raw, u + header_end, 8);
    long fn_block_size = (long)fn_block_size_raw;

    if (header_end + fn_block_size > size) {
        fprintf(stderr, "rapids_parser: function block extends past end of file\n");
        free(prog);
        return NULL;
    }

    if (parse_function_block(u + header_end, fn_block_size,
                              &prog->functions) != 0) {
        fprintf(stderr, "rapids_parser: function block parse failed\n");
        free(prog);
        return NULL;
    }

    /* ── 3. Top-level code ── */
    long code_start = header_end + fn_block_size;
    long code_end   = size;

    prog->commands = parse_commands_to_end(u, size, &code_start, code_end,
                                            &prog->commandCount);
    if (!prog->commands && code_end > code_start) {
        fprintf(stderr, "rapids_parser: top-level code parse failed\n");
        free(prog);
        return NULL;
    }

    return prog;
}

/* ── memory cleanup ─────────────────────────────────────────────────────── */

void rapids_program_free(RapidsProgram_t *prog)
{
    if (!prog) return;

    /* Header strings */
    BytecodeHeader_t *h = &prog->header;
    for (int i = 0; i < h->stringCount; i++)
        free(h->strings[i]);
    free(h->strings);

    /* Header modules */
    for (int i = 0; i < h->moduleCount; i++) {
        free(h->modules[i].moduleName);
        for (int j = 0; j < h->modules[i].importCount; j++)
            free(h->modules[i].imports[j]);
        free(h->modules[i].imports);
        free(h->modules[i].importLengths);
    }
    free(h->modules);

    /* Functions */
    RapidsProgramFunctionBlock_t *fb = &prog->functions;
    for (int i = 0; i < fb->functionCount; i++)
        free(fb->functions[i].commands);
    free(fb->functions);

    /* Top-level code */
    free(prog->commands);

    free(prog);
}

/*
 * Lightweight check for a valid RapidsLang bytecode header, without
 * allocating or fully parsing it.  Verifies:
 *   - buffer is large enough to hold signature + version + totalSize fields
 *   - signature matches
 *   - version == 0 (only supported version)
 *   - declared totalSize does not exceed the actual buffer size
 *
 * Returns 1 if the header looks valid, 0 otherwise.
 */
int has_valid_header(const uint8_t *data, long size) {
    if (!data || size < 0) return 0;

    /* Need signature (6) + version (4) + totalSize (4) at minimum. */
    if (size < SIGNATURE_LEN + 4 + 4) return 0;

    if (memcmp(data, SIGNATURE, SIGNATURE_LEN) != 0) return 0;

    long offset = SIGNATURE_LEN;

    int32_t version = read_i32(data + offset);
    offset += 4;
    if (version != 0) return 0;

    int32_t total_size = read_i32(data + offset);
    if (total_size < 0 || (long)total_size > size) return 0;

    return 1;
}
