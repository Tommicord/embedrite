/*
This is a compiler for the language "Embedrite"
Designed to be low-level, with a simple syntax
and new features such as compact structs and embedded code.

Objective:
- A compiler that generate a very lightweight binary
- For embedded systems, such as microcontrollers, where resources are limited
- Very low ram usage, with a simple syntax and no unnecessary features
- This doesn't use LLVM and Virtual Machines, it generates the binary directly,
  without any intermediate representation, so will be very fast and efficient,
- with a very small binary size, and no overhead. If C generates a 50KB binary for a simple int main() { return 0; },
  this compiler should generate a binary of around 800 bytes for the same functionality.
- The compiler uses a lot of bit packing, and other techniques to reduce the binary size

Hello world in Embedrite:

import 'ember:std';

func int main:
    std:println("Hello, World!");
    ret 0;
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#define TOKENS_SIZE 1024
#define TOKEN_SIZE 256

#define EMBEDRITE_OPERATOR 0x00
#define EMBEDRITE_KEYWORD 0x01
#define EMBEDRITE_STRING 0x02
#define EMBEDRITE_MULTILINE_COMMENT 0x03
#define EMBEDRITE_IDENTIFIER 0x04
#define EMBEDRITE_NUMBER 0x05
#define EMBEDRITE_WHITE_SPACE 0x06
#define EMBEDRITE_LINE_BREAK 0x07
#define EMBEDRITE_TOKEN 0x08
#define EMBEDRITE_DELIMITER 0x09
#define EMBEDRITE_BASE_10 0xA
#define EMBEDRITE_BASE_16 0xB
#define EMBEDRITE_BASE_2 0xC
#define EMBEDRITE_ASSEMBLY_KW 0xD

const char *embedrite_keywords[] = {
    "func", "compact", "struct",
    "int", "bit", "char", "byte",
    "embed", "and", "not", "or",
};
const char *embedrite_assembly_keywords[] = {
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
const char *embedrite_tokens[] = {
    "+", "-", "/", "$", "*", "%", "&", "|",
    "^", "!", "<<", ">>", "!&", "^|",
    "!|", ",", "\'", "\"", "(", ")", ":",
    ";", "<", ">", ">=", "<="
};
const int keywordsLength = 11;
const int assemblyKeywordsLength = 57;
const int tokensLength = 26;

struct Token {
    char *value;
    int length;
    unsigned short flags;
};

struct Tokens {
    int length;
    struct Token **arr;
};

char *Readf(const char *filename) {
    FILE *file = fopen(filename, "r");
    const int buffSize = 1024 * 1024;
    fseek(file, 0, SEEK_END);
    const int fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buff = malloc(buffSize);
    if (buff == NULL) {
        perror("Error allocating memory");
        abort();
    }
    fread(buff, sizeof(char), fileSize, file);
    fclose(file);
    return buff;
}

char *FlagsBinary(unsigned short flags) {
    const int count = sizeof(flags) * 8;
    char *sptr = malloc(flags + 1);
    for(int i = 0; i < count; ++i)
        sptr[i] = (char) ( 0x30 | ((flags >> (count - 1 - i)) & 1) );
    sptr[count] = '\0';
    return sptr;
}

void PrintTokens(struct Tokens *tokens) {
    int arrI = 0;
    while(arrI < tokens->length) {
        char *string;
        struct Token *token = tokens->arr[arrI];
        if (strcmp(token->value, "\n") == 0) {
            string = "\\n\0";
        }
        else {
            string = token->value;
        }
        char *flagss = FlagsBinary(token->flags);
        printf("%d '%s' %s\n", arrI, string, flagss);
        free(flagss);
        flagss = NULL;
        arrI++;
    }
}

// Inlining for optimization, since these functions will be called a lot of times

static inline void FlagSet(unsigned short *flags, const uint8_t flag) {
    *flags |= (1u << flag);
}

static inline void FlagClear(unsigned short *flags, const uint8_t flag) {
    *flags &= ~(1u << flag);
}

static inline unsigned int FlagRead(const unsigned short flags, const uint8_t flag) {
    return (flags & (1u << flag));
}

static inline void FlagToggle(unsigned short *flags, const uint8_t flag) {
    *flags ^= (1u << flag);
}

static inline int IsLetter(const char c) {
    return
        (c >= 0x41 && c <= 0x5A) ||
        (c >= 0x61 && c <= 0x7A);
}

static inline int IsDigit(const char c) {
    return c >= 0x30 && c <= 0x39;
}

static inline int IsHexadecimalDigit(const char c) {
    return
        IsDigit(c)               ||
        (c >= 0x41 && c <= 0x46) ||
        (c >= 0x61 && c <= 0x66);
}

static inline int IsBinaryDigit(const char c) {
    return
        c == 0x30 || c == 0x31;
}

static inline int IsType(const char *string, int length, const char *arr[]) {
    for(int i = 0; i < length; ++i) {
        if(strcmp(string, arr[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline void TokenPushChar(struct Token *token, int *tokenCharI, int *charI, const char *content, char *c) {
    token->value[(*tokenCharI)++] = *c;
    *charI += 1;
    *c = content[*charI];
}

static inline void PushToken(struct Tokens *tokens, struct Token *token) {
    tokens->arr[tokens->length] = token;
    tokens->length++;
}

static inline void FreeTokens(struct Tokens *tokens) {
    for(int i = 0; i < tokens->length; ++i) {
        free(tokens->arr[i]->value);
        tokens->arr[i]->value = NULL;
    }
    free(tokens->arr);
    free(tokens);
}

static inline void CheckLineI(const char c, int *lineI) {
    if(c == '\n')
        (*lineI)++;
}

static inline void TokenManageBase(
    struct Tokens *tokens,
    struct Token *token,
    const char *content,
    char *c,
    int *charI,
    int *tokenCharI,
    int jumpBits,
    int (*baseChecker)(char),
    unsigned short flags,
    uint8_t flag
    ) {
    FlagSet(&flags, flag);
    token->value[1] = content[*charI + 1];
    token->flags = flags;
    *charI += jumpBits, *tokenCharI += jumpBits;
    *c = content[*charI];
    while (baseChecker(*c)) {
        TokenPushChar(token, tokenCharI, charI, content, c);
    }
    token->value[*tokenCharI] = '\0';
    token->length = *tokenCharI;
    PushToken(tokens, token);
}

struct Tokens *EmberdriteGetTokens(const char *content) {
    struct Tokens *tokens = malloc(sizeof(struct Tokens));
    if (tokens == NULL) {
        goto tokens_alloc_error;
    }
    tokens->arr = malloc(TOKENS_SIZE * sizeof(struct Token *));
    if (tokens->arr == NULL) {
        goto tokens_alloc_error;
    }
    tokens->length = 0;
    int charI = 0; // Char index
    int tokenCharI = 0;
    int lineI = 0;
    unsigned short flags = 0b000000000000;
    while(content[charI] != '\0') {
        flags = 0;

        char c = content[charI];

        if (c == '/') { // Comment
            charI++;
            if(content[charI] == '/') {
                while(content[charI] != '\n' && content[charI] != '\0') {
                    charI++;
                }
                CheckLineI(content[charI], &lineI);
            }
            else if(content[charI] == '$') {
                FlagSet(&flags, EMBEDRITE_MULTILINE_COMMENT);

                while(FlagRead(flags, EMBEDRITE_MULTILINE_COMMENT)) {
                    charI++;
                    CheckLineI(content[charI], &lineI);
                    if(content[charI] == '$' && content[charI + 1] == '/') {
                        FlagClear(&flags, EMBEDRITE_MULTILINE_COMMENT);
                        charI += 2;
                    }
                }
            }
        }
        else if(IsLetter(c)) {
            struct Token *token = malloc(sizeof(struct Token));
            token->value = malloc(32);
            tokenCharI = 0;
            while(IsLetter(c)) {
                TokenPushChar(token, &tokenCharI, &charI, content, &c);
            }
            token->value[tokenCharI] = '\0';
            token->length = tokenCharI;
            char *tokenCopy = malloc(token->length);
            strncpy(tokenCopy, token->value, token->length);
            for (int i = 0; i < token->length; ++i) {
                tokenCopy[i] = (char)tolower(token->value[i]);
            }
            if (IsType(tokenCopy, assemblyKeywordsLength, embedrite_assembly_keywords)) {
                FlagSet(&flags, EMBEDRITE_KEYWORD);
                FlagSet(&flags, EMBEDRITE_ASSEMBLY_KW);
                token->flags = flags;
            }
            else if (IsType(tokenCopy, keywordsLength, embedrite_keywords)) {
                FlagSet(&flags, EMBEDRITE_KEYWORD);
                token->flags = flags;
            }
            PushToken(tokens, token);
        }
        else if(c == ' ') {
            struct Token *token = malloc(sizeof(struct Token));
            token->value = malloc(64);
            FlagSet(&flags, EMBEDRITE_WHITE_SPACE);
            token->flags = flags;
            tokenCharI = 0;
            while (content[charI] == ' ') {
                TokenPushChar(token, &tokenCharI, &charI, content, &c);
            }
            token->value[tokenCharI] = '\0';
            PushToken(tokens, token);
        }
        else if(c == '\n') {
            charI++, lineI++;
            char *lineBreak = "\n\0";
            FlagSet(&flags, EMBEDRITE_LINE_BREAK);
            struct Token *token = malloc(sizeof(struct Token));
            token->value = malloc(2);
            token->length = 1;
            strcpy(token->value, lineBreak);
            token->flags = flags;
            PushToken(tokens, token);
        }
        else if (IsDigit(c)) {
            const int jumpBits = 2;
            FlagSet(&flags, EMBEDRITE_NUMBER);
            struct Token *token = malloc(sizeof(struct Token));
            token->value = malloc(64);
            token->value[0] = c;
            tokenCharI = 0;
            if (content[charI + 1] == 'x') { // Case hexadecimal
                TokenManageBase(
                    tokens,
                    token,
                    content,
                    &c,
                    &charI,
                    &tokenCharI,
                    jumpBits,
                    IsHexadecimalDigit,
                    flags,
                    EMBEDRITE_BASE_16
                    );
            }
            else if (content[charI + 1] == 'b') {
                TokenManageBase(
                    tokens,
                    token,
                    content,
                    &c,
                    &charI,
                    &tokenCharI,
                    jumpBits,
                    IsBinaryDigit,
                    flags,
                    EMBEDRITE_BASE_2
                    );
            }
            else {
                while (IsDigit(c)) {
                    TokenPushChar(token, &tokenCharI, &charI, content, &c);
                }
                token->value[tokenCharI] = '\0';
                PushToken(tokens, token);
            }
        }
        else {
            charI++;
        }
    }

    return tokens;

    tokens_alloc_error:
        perror("Error allocation memory for tokens");
        abort();
}


int main(int argc, char *argv[]) {
    if(argc == 1) {
        perror("No input file provided");
        return 1;
    }
    char *content = Readf(argv[1]);
    struct Tokens *mytokens = EmberdriteGetTokens(content);
    PrintTokens(mytokens);
    FreeTokens(mytokens);
    free(content);
    return EXIT_SUCCESS;
}