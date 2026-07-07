#include "lexer.h"
#include <ctype.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utils/str.h"

int tokentypePrecedence(TokenType type) {
    switch(type) {
        case TOKENTYPE_OPEN_PAREN:
        case TOKENTYPE_DOT:
        case TOKENTYPE_OPEN_SQUARE:
        return 7;

        case TOKENTYPE_MULTIPLY:
        case TOKENTYPE_DIVIDE:
        case TOKENTYPE_MODULO:
        return 6;

        case TOKENTYPE_PLUS:
        case TOKENTYPE_MINUS:
        return 5;

        case TOKENTYPE_EQUALS:
        case TOKENTYPE_INEQUAL:
        return 4;

        case TOKENTYPE_GT:
        case TOKENTYPE_LT:
        case TOKENTYPE_GT_EQ:
        case TOKENTYPE_LT_EQ:
        return 3;

        case TOKENTYPE_AND:
        return 2;

        case TOKENTYPE_OR:
        return 1;

        default: return -1;
    }
}

Token_t *tokenNew(TokenType type, char *value, int len, int idx, int endIndex, Arena_t* arena) {
    Token_t *token = arenaAllocate(arena, sizeof(Token_t), alignof(Token_t));

    token->endIdx = endIndex;
    token->idx = idx;
    token->type = type;
    token->len = len;

    token->value = arenaAllocate(arena, sizeof(char) * (len + 1), alignof(char));
    memcpy(token->value, value, len * sizeof(char));
    token->value[len] = '\0';

    return token;
}

static char* keywordStrings[] = {
    "export",
    "if",
    "for",
    "while",
    "let",
    "const",
    "null",
    "use",
    "true",
    "false",
    "return",
    "break",
    "continue",
    "define",
    "else"
};

static TokenType keywordTokenTypes[] = {
    TOKENTYPE_EXPORT,
    TOKENTYPE_IF,
    TOKENTYPE_FOR,
    TOKENTYPE_WHILE,
    TOKENTYPE_LET,
    TOKENTYPE_CONST,
    TOKENTYPE_NULL,
    TOKENTYPE_USE,
    TOKENTYPE_TRUE,
    TOKENTYPE_FALSE,
    TOKENTYPE_RETURN,
    TOKENTYPE_BREAK,
    TOKENTYPE_CONTINUE,
    TOKENTYPE_DEFINE,
    TOKENTYPE_ELSE
};

static char* symbolStrings[] = {
    ".",
    ",",
    ":",
    ";",
    "^",
    "&&",
    "&",
    "+",
    "-",
    "/",
    "*",
    "%",
    "==",
    "=",
    "!=",
    "!",
    "<=",
    "<",
    ">=",
    ">",
    "||",
    "{",
    "}",
    "(",
    ")",
    "[",
    "]",
};

static TokenType symbolTokenTypes[] = {
    TOKENTYPE_DOT,
    TOKENTYPE_COMMA,
    TOKENTYPE_COLON,
    TOKENTYPE_SEMICOLON,
    TOKENTYPE_CARET,
    TOKENTYPE_AND,
    TOKENTYPE_AMPERSAND,
    TOKENTYPE_PLUS,
    TOKENTYPE_MINUS,
    TOKENTYPE_DIVIDE,
    TOKENTYPE_MULTIPLY,
    TOKENTYPE_MODULO,
    TOKENTYPE_EQUALS,
    TOKENTYPE_ASSIGNMENT,
    TOKENTYPE_INEQUAL,
    TOKENTYPE_NOT,
    TOKENTYPE_LT_EQ,
    TOKENTYPE_LT,
    TOKENTYPE_GT_EQ,
    TOKENTYPE_GT,
    TOKENTYPE_OR,
    TOKENTYPE_OPEN_CURLY,
    TOKENTYPE_CLOSED_CURLY,
    TOKENTYPE_OPEN_PAREN,
    TOKENTYPE_CLOSED_PAREN,
    TOKENTYPE_OPEN_SQUARE,
    TOKENTYPE_CLOSED_SQUARE,
};

#define isNumber(c) (c >= '0' && c <= '9')

LexingResult_t *lex(char *code, Arena_t* arena) {
    List_t *list = listNew();

    int i;
    for(i = 0; code[i] != '\0'; i++) {
        char c = code[i];

        if(c == ' ' || c == '\n' || c == '\r') {
            continue;
        }

        bool shouldContinue = false;
        for(int j = 0; j < sizeof(keywordStrings) / sizeof(keywordStrings[0]); j++) {
            if(startswith(&code[i], keywordStrings[j])) {
                int length = strlen(keywordStrings[j]);
                if(isalnum(code[i+length]) || code[i+length] == '_')
                    continue;

                listAppend(list, tokenNew(keywordTokenTypes[j], &code[i], length, i, i + length, arena));
                i += length - 1;
                shouldContinue = true;
                break;
            }
        }
        if(shouldContinue) {
            continue;
        }


        if(
            // is #
            isNumber(c)||
            // is -# or -.#
            (c == '-' && ((code[i+1] == '.' && isNumber(code[i+2])) || isNumber(code[i + 1]))) ||
            // is .#
            (c == '.' && isNumber(code[i+1]))
        ) {
            bool alreadyHasDot = false;
            int j;
            for(j = 1; code[i + j] != '\0'; j++) {
                char cnext = code[i + j];
                if(!(isNumber(cnext)) && cnext != '.') {
                    break;
                }
                if(cnext == '.') {
                    if(alreadyHasDot) {
                        break; // weird behavior but I'm going to let 1.0.toString() work, even if it looks gross.
                    }
                    alreadyHasDot = true;
                }
            }

            listAppend(list, tokenNew(TOKENTYPE_LITERAL_NUMBER, &code[i], j, i, i+j, arena));
            i += j - 1;

            continue;
        }

        for(int j = 0; j < sizeof(symbolStrings) / sizeof(symbolStrings[0]); j++) {
            if(startswith(&code[i], symbolStrings[j])) {
                int length = strlen(symbolStrings[j]);
                listAppend(list, tokenNew(symbolTokenTypes[j], &code[i], length, i, i + length, arena));
                i += length - 1;
                shouldContinue = true;
                break;
            }
        }
        if(shouldContinue) {
            continue;
        }

        if(c == '`' || c == '"' || c == '\'') {
            listAppend(list, tokenNew(TOKENTYPE_STRING_START, &code[i], 1, i, i+1, arena));

            int contentStart = i + 1;
            int strI = 1;

            while(true) {
                char cur = code[i + strI];

                if(cur == '\0') {
                    // todo: emit some error or something
                    break;
                }

                // Count how many backslashes immediately precede this character,
                // so we correctly handle runs like \\\" (escaped backslash + real quote).
                int backslashRun = 0;
                while(i + strI - 1 - backslashRun >= 0 && code[i + strI - 1 - backslashRun] == '\\') {
                    backslashRun++;
                }
                bool isEscaped = (backslashRun % 2) == 1;

                if(cur == c && !isEscaped) {
                    // Closing quote found. Emit any trailing literal content first.
                    if(strI > contentStart - i) {
                        int contentLen = (i + strI) - contentStart;
                        listAppend(list, tokenNew(TOKENTYPE_STRING_CONTENT, &code[contentStart], contentLen, contentStart, i + strI, arena));
                    }
                    listAppend(list, tokenNew(TOKENTYPE_STRING_END, &code[i + strI], 1, i + strI, i + strI + 1, arena));
                    i = i + strI; // land on the closing quote; outer loop's i++ moves past it
                    break;
                }

                if(cur == '{' && !isEscaped) {
                    // Emit literal content accumulated so far.
                    if(i + strI > contentStart) {
                        int contentLen = (i + strI) - contentStart;
                        listAppend(list, tokenNew(TOKENTYPE_STRING_CONTENT, &code[contentStart], contentLen, contentStart, i + strI, arena));
                    }
                    listAppend(list, tokenNew(TOKENTYPE_OPEN_CURLY, &code[i + strI], 1, i + strI, i + strI + 1, arena));

                    // Find the matching unescaped '}', respecting nested braces.
                    int depth = 1;
                    int exprStart = i + strI + 1;
                    int k = exprStart;
                    while(code[k] != '\0' && depth > 0) {
                        if(code[k] == '{') depth++;
                        else if(code[k] == '}') depth--;
                        if(depth == 0) break;
                        k++;
                    }

                    if(code[k] == '\0') {
                        // Unterminated interpolation expression.
                        break;
                    }

                    // Recursively lex just the interior expression.
                    int exprLen = k - exprStart;
                    char *exprBuf = malloc(exprLen + 1);
                    memcpy(exprBuf, &code[exprStart], exprLen);
                    exprBuf[exprLen] = '\0';

                    LexingResult_t *subResult = lex(exprBuf, arena);
                    for(int tokIdx = 0; tokIdx < subResult->tokens->len; tokIdx++) {
                        Token_t *tok = subResult->tokens->items[tokIdx];
                        tok->idx += i + strI + 1;
                        tok->endIdx += i + strI + 1;
                        listAppend(list, tok);
                    }
                    listFree(subResult->tokens);
                    free(subResult);
                    free(exprBuf);

                    listAppend(list, tokenNew(TOKENTYPE_CLOSED_CURLY, &code[k], 1, k, k + 1, arena));

                    // Resume accumulating string content right after the closing '}'.
                    contentStart = k + 1;
                    strI = (k + 1) - i;
                    continue;
                }

                strI++;
            }
            continue;
        }

        if(isalpha(c)) {
            int j;
            for(j = 1; code[i+j] != '\0' && (isalnum(code[i+j]) || code[i+j] == '_'); j++);

            listAppend(list, tokenNew(TOKENTYPE_IDENTIFIER, &code[i], j, i, i+j, arena));
            i += j - 1;
        }
    }

    LexingResult_t *res = malloc(sizeof(LexingResult_t));
    res->tokens = list;
    res->charsConsumed = i;

    return res;
}


static const char *token_type_to_string(TokenType t)
{
    /* Switch is the most portable way; a static array works too if you
     * know the enum values are contiguous.
     */
    switch (t) {
        case TOKENTYPE_EXPORT:           return "EXPORT";
        case TOKENTYPE_IF:               return "IF";
        case TOKENTYPE_FOR:              return "FOR";
        case TOKENTYPE_WHILE:            return "WHILE";
        case TOKENTYPE_LET:              return "LET";
        case TOKENTYPE_CONST:            return "CONST";
        case TOKENTYPE_NULL:             return "NULL";
        case TOKENTYPE_USE:              return "USE";
        case TOKENTYPE_TRUE:             return "TRUE";
        case TOKENTYPE_FALSE:            return "FALSE";
        case TOKENTYPE_RETURN:           return "RETURN";
        case TOKENTYPE_BREAK:            return "BREAK";
        case TOKENTYPE_CONTINUE:         return "CONTINUE";
        case TOKENTYPE_DEFINE:           return "DEFINE";
        case TOKENTYPE_ELSE:             return "ELSE";

        /* Symbols */
        case TOKENTYPE_DOT:              return ".";
        case TOKENTYPE_COMMA:            return ",";
        case TOKENTYPE_COLON:            return ":";
        case TOKENTYPE_SEMICOLON:        return ";";
        case TOKENTYPE_CARET:            return "^";
        case TOKENTYPE_AMPERSAND:        return "&";

        /* Operators */
        case TOKENTYPE_PLUS:             return "+";
        case TOKENTYPE_MINUS:            return "-";
        case TOKENTYPE_DIVIDE:           return "/";
        case TOKENTYPE_MULTIPLY:         return "*";
        case TOKENTYPE_MODULO:           return "%";
        case TOKENTYPE_ASSIGNMENT:       return "=";
        case TOKENTYPE_NOT:              return "!";

        /* Comparison operators */
        case TOKENTYPE_EQUALS:           return "==";
        case TOKENTYPE_LT:               return "<";
        case TOKENTYPE_LT_EQ:            return "<=";
        case TOKENTYPE_GT:               return ">";
        case TOKENTYPE_GT_EQ:            return ">=";
        case TOKENTYPE_INEQUAL:          return "!=";
        case TOKENTYPE_AND:              return "&&";
        case TOKENTYPE_OR:               return "||";

        /* Braces / parens */
        case TOKENTYPE_OPEN_CURLY:       return "{";
        case TOKENTYPE_CLOSED_CURLY:     return "}";
        case TOKENTYPE_OPEN_PAREN:       return "(";
        case TOKENTYPE_CLOSED_PAREN:     return ")";
        case TOKENTYPE_OPEN_SQUARE:      return "[";
        case TOKENTYPE_CLOSED_SQUARE:    return "]";

        /* Others */
        case TOKENTYPE_IDENTIFIER:       return "IDENTIFIER";
        case TOKENTYPE_LITERAL_NUMBER:   return "NUMBER";
        case TOKENTYPE_STRING_START:     return "\"(start)";
        case TOKENTYPE_STRING_CONTENT:   return "STRING_CONTENT";
        case TOKENTYPE_STRING_END:       return "\"(end)";

        default:
            return "<UNKNOWN_TOKEN>";
    }
}

void print_token(const Token_t *tok)
{
    if (!tok) {
        printf("<NULL token pointer>\n");
        return;
    }

    /* Base description: type name, position, length. */
    const char *typeName = token_type_to_string(tok->type);
    printf("Token %-25s [idx=%3d, endIdx=%3d, len=%2d]",
           typeName,
           tok->idx,
           tok->endIdx,
           tok->len);

    /* If the token carries a value (e.g. identifiers or literals) show it.
     * We guard against NULL to avoid crashes on tokens that don't have
     * an attached string, e.g. punctuation.
     */
    if (tok->value && *tok->value != '\0') {
        /* Escape any non‑printable characters for safety */
        printf("  value=\"");
        const char *p = tok->value;
        while (*p) {
            unsigned char c = *p++;
            if (c == '\\' || c == '\"')
                putchar('\\'), putchar(c);
            else if (isprint(c))
                putchar(c);
            else
                printf("\\x%02X", c);   /* hex escape for non‑printable */
        }
        printf("\"");
    }

    puts("");   /* newline at the end of the token description */
}
