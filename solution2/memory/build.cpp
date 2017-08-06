#include <stdlib.h>
#include <stdio.h>
#include "memory.h"

static void **space = NULL;

void** alloc_memory_space(int max_x, int max_y, int max_z)
{
    void **m_space = NULL;

    if (!max_x || !max_y || !max_z)
        return NULL;

    m_space = (void**)calloc(max_x * max_y * max_z, sizeof(void*));
    if (!m_space)
        printf("calloc failed.\n");

    return m_space;
}

int rebuild_memory(void *arg)
{
    int x, y, z;
    int ret = 0;

    space = alloc_memory_space(MAX_X, MAX_Y, MAX_Z);
    if (!space) {
        ret = -1;
        printf("Alloc memory space failed.\n");
        goto out;
    }
    space[POS(1,2,3)] = (void*)5;

    for (x = 0; x < MAX_X; x++)
        for (y = 0; y < MAX_Y; y++)
            for (z = 0; z < MAX_Z; z++)
                if (space[POS(x,y,z)])
                    printf(".");

out:
    return ret;
}

void destroy_memory()
{
    printf("%s\n", __func__);
    if (space)
        free(space);
}
