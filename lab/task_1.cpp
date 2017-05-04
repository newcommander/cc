#include <iostream>
#include "task.h"

bool task_should_stop(void *arg)
{
	return true;
}

void task_1_init(void *arg)
{
    Task *task = (Task*)arg;

    if (!task)
        return;

    std::cout << task->name << ": init" << std::endl;
}

void task_1_run(void *arg)
{
    Task *task = (Task*)arg;

    if (!task)
        return;

    std::cout << task->name << ": run" << std::endl;
}

void task_1_done(void *arg)
{
    Task *task = (Task*)arg;

    if (!task)
        return;

    std::cout << task->name << ": done" << std::endl;
}

Task task_1("task_1", 0x1, task_should_stop, task_1_init, task_1_run, task_1_done);
Task task_2("task_2", 0x2, task_should_stop, task_1_init, task_1_run, task_1_done);
Task task_3("task_3", 0x3, task_should_stop, task_1_init, task_1_run, task_1_done);
Task task_4("task_4", 0x4, task_should_stop, task_1_init, task_1_run, task_1_done);
