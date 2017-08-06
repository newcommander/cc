#include <stdio.h>

#include <memory/memory.h>
#include <mind/mind.h>
#include <perceptron/perceptron.h>
#include <action/action.h>

int main(int argc, char **argv)
{
    rebuild_memory(NULL);
    destroy_memory();
    rebuild_mind(NULL);
    resetup_perceptron(NULL);
    resetup_action(NULL);
    return 0;
}
