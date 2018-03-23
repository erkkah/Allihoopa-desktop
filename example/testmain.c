#include "allihoopa.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

const char* slurp(const char* path) {
    FILE* file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    fread(buffer, size, 1, file);
    buffer[size] = 0;
    return buffer;
}

void waitForEnter() {
    printf("Press enter...");
    getchar();
}

int main() {
    {
        const char* setupRequest = slurp("setup.json");
        assert(setupRequest != NULL);
        int result = AHsetup(setupRequest, strlen(setupRequest));
        if (result) {
            printf("AHsetup returned %d: \"%s\"\n", result, AHerrorCodeToMessage(result));
            return result;
        }
    }

    waitForEnter();

    {
        const char* minimalDropRequest = slurp("minimaldrop.json");
        assert(minimalDropRequest != NULL);
        int result = AHdrop(minimalDropRequest, strlen(minimalDropRequest), 42);
        if (result) {
            printf("AHsetup (minimal) returned %d: \"%s\"\n", result, AHerrorCodeToMessage(result));
            return result;
        }
    }

    waitForEnter();

    if(0){
        const char* fullDropRequest = slurp("fulldrop.json");
        assert(fullDropRequest != NULL);
        int result = AHdrop(fullDropRequest, strlen(fullDropRequest), 43);
        if (result) {
            printf("AHsetup (full) returned %d: \"%s\"\n", result, AHerrorCodeToMessage(result));
            return result;
        }
    }

    if(0){
        int result = AHclose();
        if(result) {
            printf("AHclose returned %d: \"%s\"\n", result, AHerrorCodeToMessage(result));
            return result;        
        }
    }

    return 0;
}
