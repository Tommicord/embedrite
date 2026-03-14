#include "lexan.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMBDC_MANAGE_CHUNK_ALLOC(tokenCharPos) \
    if (tokenCharPos + 1 >= token->allocated) { \
        token->allocated = tokenCharPos + 128; \
        char *chunk = realloc(token->value, token->allocated); \
        if(chunk == NULL) { \
            free(token->value); \
            perror("Failed allocating memory from token"); \
            abort(); \
        } \
    token->value = chunk; \
    }

static void FlagSet(FLAGS *flags, const uint8_t flag) {
    *flags |= (1u << flag);
}

static void FlagClear(FLAGS *flags, const uint8_t flag) {
    *flags &= ~(1u << flag);
}

static uint8_t FlagRead(const FLAGS flags, const uint8_t flag) {
    return (flags & (1u << flag));
}

static int IsLetter(const char c) {
    return
        (c >= 0x41 && c <= 0x5A) ||
        (c >= 0x61 && c <= 0x7A);
}

static int IsDigit(const char c) {
    return c >= 0x30 && c <= 0x39;
}

static int IsHexadecimalDigit(const char c) {
    return
        IsDigit(c)               ||
        (c >= 0x41 && c <= 0x46) ||
        (c >= 0x61 && c <= 0x66);
}

static int IsBinaryDigit(const char c) {
    return
        c == 0x30 || c == 0x31;
}

static int IsDelimiter(const char c) {
    return
            c == 0x20
            || c >= 0x27 && c <= 0x2E
            || c == 0x3A
            || c == 0x3B;
}

static int DefinedToken(const char *string, int length, const char *arr[]) {
    for(int i = 0; i < length; ++i) {
        if(strcmp(string, arr[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void TokenPushChar(struct EmbdcToken *token, int *tokenCharPos, int *pos, const char *content, char *c) {
    token->value[(*tokenCharPos)++] = *c;
    *pos += 1;
    *c = content[*pos];
}

static void PushToken(TOKENS tokens, struct EmbdcToken *token) {
    tokens->arr[tokens->length] = token;
    tokens->length++;
}

static void CheckLineI(const char c, int *line) {
    if(c == '\n')
        (*line)++;
}

static void TokenManageBase(
    TOKENS tokens,
    struct EmbdcToken *token,
    const char *content,
    char *c,
    int *pos,
    int *tokenCharPos,
    int jumpBits,
    int (*checker)(char),
    FLAGS flags,
    uint8_t flag
    ) {
    FlagSet(&flags, flag);
    token->value[1] = content[*pos + 1];
    token->flags = flags;
    *pos += jumpBits, *tokenCharPos += jumpBits;
    *c = content[*pos];
    while (checker(*c)) {
        EMBDC_MANAGE_CHUNK_ALLOC(*tokenCharPos);
        TokenPushChar(token, tokenCharPos, pos, content, c);
    }
    token->value[*tokenCharPos] = '\0';
    token->length = *tokenCharPos;
    PushToken(tokens, token);
}

static struct EmbdcToken *TokenDirect(const char *value, FLAGS flags) {
    int length = strlen(value);
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->value = malloc(length + 1);
    strcpy(token->value, value);
    token->flags = flags;
    token->length = length;
    return token;
}

static void EmbdcReadComment(
    int *pos,
    int *line,
    const char *content,
    FLAGS flags
    ) {
    (*pos)++;
    if(content[*pos] == '/') {
        while(content[*pos] != '\n' && content[*pos] != '\0') {
            (*pos)++;
        }
        CheckLineI(content[*pos], line);
    }
    else if(content[*pos] == '$') {
        FlagSet(&flags, EMBEDRITE_COMMENT);

        while(FlagRead(flags, EMBEDRITE_COMMENT)) {
            (*pos)++;
            CheckLineI(content[*pos], line);
            if(content[*pos] == '$' && content[*pos + 1] == '/') {
                FlagClear(&flags, EMBEDRITE_COMMENT);
                *pos += 2;
            }
        }
    }
}

static void EmbdcReadWord(
    char *c,
    int *pos,
    int *tokenCharPos,
    const char *content,
    TOKENS dist,
    FLAGS flags
    ) {
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->allocated = 32;
    token->value = malloc(token->allocated);
    *tokenCharPos = 0;
    while(IsLetter(*c)) {
        EMBDC_MANAGE_CHUNK_ALLOC(*tokenCharPos);
        TokenPushChar(token, tokenCharPos, pos, content, c);
    }
    token->value[*tokenCharPos] = '\0';
    token->length = *tokenCharPos;
    char *tokenCopy = malloc(token->length);
    strncpy(tokenCopy, token->value, token->length);
    for (int i = 0; i < token->length; ++i) {
        tokenCopy[i] = (char)tolower(token->value[i]);
    }
    if (DefinedToken(tokenCopy, assemblyKeywordsLength, embedrite_assembly_keywords)) {
        FlagSet(&flags, EMBEDRITE_KEYWORD);
        FlagSet(&flags, EMBEDRITE_ASSEMBLY_KW);
        token->flags = flags;
    }
    else if (DefinedToken(tokenCopy, keywordsLength, embedrite_keywords)) {
        FlagSet(&flags, EMBEDRITE_KEYWORD);
        token->flags = flags;
    }
    free(tokenCopy);
    tokenCopy = NULL;
    PushToken(dist, token);
}

static void EmbdcReadSpace(
    char *c,
    int *pos,
    int *tokenCharPos,
    const char *content,
    TOKENS dist,
    FLAGS flags
    ) {
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->allocated = 64;
    token->value = malloc(token->allocated);
    FlagSet(&flags, EMBEDRITE_WHITE_SPACE);
    token->flags = flags;
    *tokenCharPos = 0;
    while (content[*pos] == ' ') {
        EMBDC_MANAGE_CHUNK_ALLOC(*tokenCharPos);
        TokenPushChar(token, tokenCharPos, pos, content, c);
    }
    token->length = *tokenCharPos;
    token->value[*tokenCharPos] = '\0';
    PushToken(dist, token);
}

static void EmbdcReadString(
    char *c,
    int *pos,
    int *tokenCharPos,
    const char *content,
    TOKENS dist,
    FLAGS flags) {
    (*pos)++;
    *c = content[*pos];
    PushToken(dist, TokenDirect(c, EMBEDRITE_TOKEN));
    FlagSet(&flags, EMBEDRITE_STRING);
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->allocated = 128;
    token->value = malloc(token->allocated);
    token->length = 1; // Just the first "
    token->flags = flags;
    *tokenCharPos = 0;
    while(content[*pos] != '"' && content[*pos] != '\0') {
        EMBDC_MANAGE_CHUNK_ALLOC(*tokenCharPos);
        TokenPushChar(token, tokenCharPos, pos, content, c);
    }
    token->length = *tokenCharPos;
    (*pos)++;
    token->value[*tokenCharPos] = '\0';
    PushToken(dist, token);
}

static void EmbdcReadLineBreak(
    int *charI,
    int *lineI,
    TOKENS dist,
    FLAGS flags) {
    (*charI)++, (*lineI)++;
    const char *lineBreak = "\n\0";
    FlagSet(&flags, EMBEDRITE_LINE_BREAK);
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->value = malloc(2);
    token->length = 1;
    strcpy(token->value, lineBreak);
    token->flags = flags;
    PushToken(dist, token);
}

static void EmbdcReadNumber(
    char *c,
    int *pos,
    int *tokenCharPos,
    const char *content,
    TOKENS dist,
    FLAGS flags
    ) {
    const int jumpChars = 2;
    FlagSet(&flags, EMBEDRITE_NUMBER);
    struct EmbdcToken *token = malloc(sizeof(struct EmbdcToken));
    token->value = malloc(64);
    token->value[0] = *c;
    *tokenCharPos = 0;
    if (content[*pos + 1] == 'x') { // Case hexadecimal
        TokenManageBase(
            dist,
            token,
            content,
            c,
            pos,
            tokenCharPos,
            jumpChars,
            IsHexadecimalDigit,
            flags,
            EMBEDRITE_BASE_16
            );
    }
    else if (content[*pos + 1] == 'b') {
        TokenManageBase(
            dist,
            token,
            content,
            c,
            pos,
            tokenCharPos,
            jumpChars,
            IsBinaryDigit,
            flags,
            EMBEDRITE_BASE_2
            );
    }
    else {
        while (IsDigit(*c)) {
            TokenPushChar(token, tokenCharPos, pos, content, c);
        }
        token->value[*tokenCharPos] = '\0';
        PushToken(dist, token);
    }
}

static void EmbdcReadDelmt(
    char *c,
    int *pos,
    TOKENS dist,
    FLAGS flags
    ) {
    char *delmt = malloc(2); // The delimiter is only 2 bytes
    delmt[0] = *c;
    delmt[1] = '\0';
    FlagSet(&flags, EMBEDRITE_DELIMITER);
    PushToken(dist, TokenDirect(delmt, flags));
    (*pos)++;
}

static void EmbdcReadNDT(
    char *c,
    int *pos,
    TOKENS dist,
    FLAGS flags) {
    FlagSet(&flags, EMBEDRITE_NDT);
    PushToken(dist, TokenDirect(c, flags));
    (*pos)++;
}

TOKENS EmbdcGetTokens(const char *content) {
    struct EmbdcTokens *tokens = malloc(sizeof(struct EmbdcTokens));
    tokens->arr = malloc(TOKENS_SIZE * sizeof(struct EmbdcToken *));
    tokens->length = 0;
    int charI = 0; // Char index
    int tokenCharI = 0;
    int lineI = 0;
    FLAGS flags = 0b000000000000;
    while(content[charI] != '\0') {
        flags = 0;

        char c = content[charI];

        if (c == '/') { // Comment
            EmbdcReadComment(
                &charI,
                &lineI,
                content,
                flags
                );
        }
        else if(IsLetter(c)) {
            EmbdcReadWord(
                &c,
                &charI,
                &tokenCharI,
                content,
                tokens,
                flags);
        }
        else if(c == ' ') {
            EmbdcReadSpace(
                &c,
                &charI,
                &tokenCharI,
                content,
                tokens,
                flags);
        }
        else if(c == '"') {
            EmbdcReadString(
                &c,
                &charI,
                &tokenCharI,
                content,
                tokens,
                flags
                );
        }
        else if(c == '\n') {
            EmbdcReadLineBreak(
                &charI,
                &lineI,
                tokens,
                flags);
        }
        else if (IsDelimiter(c)) {
            EmbdcReadDelmt(
                &c,
                &charI,
                tokens,
                flags);
        }
        else if (IsDigit(c)) {
            EmbdcReadNumber(
                &c,
                &charI,
                &tokenCharI,
                content,
                tokens,
                flags);
        }
        else {
            EmbdcReadNDT(
                &c,
                &charI,
                tokens,
                flags);
        }
    }

    return tokens;
}

void EmbdcFreeTokens(TOKENS tokens) {
    for(int i = 0; i < tokens->length; ++i) {
        free(tokens->arr[i]->value);
        tokens->arr[i]->value = NULL;
    }
    free(tokens->arr);
    free(tokens);
}

