#include "helpers.h"

#include <stdio.h>

void strcpy_safe(char *destination, const char *source, size_t max_bytes)
{
    snprintf(destination, max_bytes, "%s", source);
}
