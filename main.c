/*
Embedrite:
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

import 'embdr:std' std;

func int main:
    std:println("Hello, World!");
    ret 0;
*/

#include <stdio.h>
#include <stdlib.h>

#define EMBDC_DEBUG
#include "src/embedrite/lexa.c"

char *Readf(const char *filename) {
    FILE *file = fopen(filename, "r");
    const int buffSize = 1024 * 1024;
    char *buff = malloc(buffSize);
    if (buff == NULL) {
        perror("Error allocating memory");
        abort();
    }
    fseek(file, 0, SEEK_END);
    const int length = ftell(file);
    fseek(file, 0, SEEK_SET);
    fread(buff, sizeof(char), length, file);
    fclose(file);
    return buff;
}

int main(int argc, char *argv[]) {
    if(argc == 1) {
        perror("No input file provided");
        return 1;
    }

    char *content = Readf(argv[1]);
    struct EmbdcTokens *mytokens = EmbdcGetTokens(content);
    EMBDC_TOKENS_PRINT(mytokens);
    EmbdcFreeTokens(mytokens);
    free(content);
    return EXIT_SUCCESS;
}