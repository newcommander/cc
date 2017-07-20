#include <iostream>
#include "action.h"

void action_1_init(void *arg)
{
    Action *action = (Action*)arg;

    if (!action)
        return;

    std::cout << action->name << ": init" << std::endl;
}

void action_1_run(void *arg)
{
    Action *action = (Action*)arg;

    if (!action)
        return;

    std::cout << action->name << ": run" << std::endl;
}

void action_1_done(void *arg)
{
    Action *action = (Action*)arg;

    if (!action)
        return;

    std::cout << action->name << ": done" << std::endl;
}

Action action_1("action_1", 0x1, action_1_init, action_1_run, action_1_done);
Action action_2("action_2", 0x2, action_1_init, action_1_run, action_1_done);
Action action_3("action_3", 0x3, action_1_init, action_1_run, action_1_done);
Action action_4("action_4", 0x4, action_1_init, action_1_run, action_1_done);
