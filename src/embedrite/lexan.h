#ifndef EMBEDRITE_LEXA_H_
#define EMBEDRITE_LEXA_H_

#include <string.h>

#define TOKENS_SIZE 1024
#define TOKEN_SIZE 256

#define EMBEDRITE_OPERATOR 0x00
#define EMBEDRITE_KEYWORD 0x01
#define EMBEDRITE_STRING 0x02
#define EMBEDRITE_COMMENT 0x03
#define EMBEDRITE_IDENTIFIER 0x04
#define EMBEDRITE_NUMBER 0x05
#define EMBEDRITE_WHITE_SPACE 0x06
#define EMBEDRITE_LINE_BREAK 0x07
#define EMBEDRITE_TOKEN 0x08
#define EMBEDRITE_DELIMITER 0x09
#define EMBEDRITE_BASE_10 0xA
#define EMBEDRITE_BASE_16 0xB
#define EMBEDRITE_BASE_2 0xC
#define EMBEDRITE_ASSEMBLY_KW 0xE
#define EMBEDRITE_MULTILINE 0xF
#define EMBEDRITE_NDT 0x18 // Not defined token

typedef struct EmbdcTokens *TOKENS;
typedef unsigned short FLAGS;

struct EmbdcToken {
    char *value;
    int length;
    int allocated;
    unsigned short flags;
};

struct EmbdcTokens {
    int length;
    struct EmbdcToken **arr;
};

static const char *embedrite_keywords[] = {
    "func", "compact", "struct", "ret",
    "int", "bit", "char", "byte", "exit",
    "embed", "and", "not", "or",
};
static const char *embedrite_assembly_keywords[] = {
    "mov", "push", "pop", "lea",
    "add", "sub", "inc", "dec",
    "imul", "idiv", "adc", "and", "not", "andl", "notl", "orl",
    "neg", "shl", "shr", "jmp",
    "je", "jne", "jz", "jg",
    "jge", "jl", "jle", "cmp",
    "js", "jnz", "jns", "js", "jnc",
    "enter", "leave", "loop", "loope",
    "loopne", "loopnz", "loopz",
    "call", "ret", "movzx",
    "test", "nop", "movsb",
    "movsw", "movw", "movl",
    "movw", "movb", "movsbw",
    "movsbl", "movsbw", "pusha",
    "popa", "xchg", "xlat"
};
static const char *embedrite_tokens[] = {
    "+", "-", "/", "$", "*", "%", "&", "|",
    "^", "!", "<<", ">>",
    ",", "\'", "\"", "(", ")", ":",
    ";", "<", ">", ">=", "<="
};

static const int keywordsLength = 11;
static const int assemblyKeywordsLength = 57;
static const int tokensLength = 26;

#ifdef EMBDC_DEBUG
#define EMBDC_FLAGS_BINARY(flags) \
    const int count = sizeof(flags) * 8; \
    char *sptr = malloc(flags + 1); \
    for(int i = 0; i < count; ++i) \
        sptr[i] = (char) ( 0x30 | ((flags >> (count - 1 - i)) & 1) ); \
    sptr[count] = '\0'; \
    printf("%s", sptr); \
    free(sptr);

#define EMBDC_TOKENS_PRINT(tokens) \
    int arrI = 0; \
    while(arrI < tokens->length) { \
        char *string; \
        struct EmbdcToken *token = tokens->arr[arrI]; \
        if (strcmp(token->value, "\n") == 0) { \
            string = "\\n\0"; \
        } \
        else { \
            string = token->value; \
        } \
        printf("%d '%s'\n", arrI, string); \
        arrI++; \
    }
#endif

TOKENS EmbdcTokenize(const char *content);
void EmbdcFreeTokens(TOKENS tokens);

#endif