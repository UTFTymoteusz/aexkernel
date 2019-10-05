#pragma once

int strcmp(char* left, char* right) {
    int i = 0;

    while (true) {
        if (left[i] == right[i]) {
            if (left[i] == '\0')
                return 0;
        }
        else
            return left[i] < right[i] ? -1 : 1;
        
        ++i;
    }
    return 0;
}