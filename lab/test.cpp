#include <iostream>
#include <stdio.h>
#include "common.h"
#include "timing.h"

int main(int argc, char **argv)
{
    int i = 23;

    printf("%d\n", i % 10);
    start_timing_wheel();
    msleep(1000);
    stop_timing_wheel();

    return 0;
}
