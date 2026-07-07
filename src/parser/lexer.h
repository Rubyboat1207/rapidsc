#include "../utils/list.h"
#include "../utils/arena.h"
#include <stddef.h>
#ifndef LEXER
#define LEXER

typedef enum {
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
    TOKENTYPE_ELSE,

    // Symbols
    TOKENTYPE_DOT,
    TOKENTYPE_COMMA,
    TOKENTYPE_COLON,
    TOKENTYPE_SEMICOLON,
    TOKENTYPE_CARET,
    TOKENTYPE_AMPERSAND,

    // Operators
    TOKENTYPE_PLUS,
    TOKENTYPE_MINUS,
    TOKENTYPE_DIVIDE,
    TOKENTYPE_MULTIPLY,
    TOKENTYPE_MODULO,
    TOKENTYPE_ASSIGNMENT,
    TOKENTYPE_NOT,

    // Comparison Operators
    TOKENTYPE_EQUALS,
    TOKENTYPE_LT,
    TOKENTYPE_LT_EQ,
    TOKENTYPE_GT,
    TOKENTYPE_GT_EQ,
    TOKENTYPE_INEQUAL,
    TOKENTYPE_AND,
    TOKENTYPE_OR,

    TOKENTYPE_OPEN_CURLY,
    TOKENTYPE_CLOSED_CURLY,
    TOKENTYPE_OPEN_PAREN,
    TOKENTYPE_CLOSED_PAREN,
    TOKENTYPE_OPEN_SQUARE,
    TOKENTYPE_CLOSED_SQUARE,

    TOKENTYPE_IDENTIFIER,
    TOKENTYPE_LITERAL_NUMBER,

    TOKENTYPE_STRING_START,
    TOKENTYPE_STRING_CONTENT,
    TOKENTYPE_STRING_END
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int idx;
    int endIdx;
    int len;
} Token_t;

typedef struct {
    List_t *tokens;
    int charsConsumed;
} LexingResult_t;

int tokentypePrecedence(TokenType type);

Token_t *tokenNew(TokenType type, char *value, int len, int idx, int endIndex, Arena_t* arena);

LexingResult_t* lex(char* code, Arena_t* arena);
void print_token(const Token_t *tok);
#endif
