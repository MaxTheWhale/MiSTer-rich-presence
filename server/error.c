#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void error_check(int error, const char message[])
{
    if (error < 0)
    {
        perror(message);
        exit(EXIT_FAILURE);
    }
}
