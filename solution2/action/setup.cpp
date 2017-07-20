#include <stdio.h>

int resetup_action(void *arg)
{
    printf("%s\n", __func__);
    return 0;
}
