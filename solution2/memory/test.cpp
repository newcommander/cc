#include <stdio.h>

#include "memory.h"

int main(int argc, char **argv)
{
    rebuild_memory(NULL);
    destroy_memory();

    return 0;
}
