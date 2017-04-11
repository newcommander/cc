#include <string>
#include <vector>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"

void main_task_init(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_INIT;
    printf("[%s] init.\n", task->name.c_str());
}

void main_task_run(void *arg)
{
    struct Task *task = (struct Task*)arg;
    size_t i;

    task->state = TASK_STATE_RUN;
    printf("[%s] run.\n", task->name.c_str());

    for (i = 0; i < task->sub_tasks.size(); i++) {
        if (task->sub_tasks[i]->task_init)
            task->sub_tasks[i]->task_init(task->sub_tasks[i]);
        if (task->sub_tasks[i]->task_run)
            task->sub_tasks[i]->task_run(task->sub_tasks[i]);
        if (task->sub_tasks[i]->task_done)
            task->sub_tasks[i]->task_done(task->sub_tasks[i]);
    }
}

void main_task_done(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_DONE;
    printf("[%s] done.\n", task->name.c_str());
}

void sub_task_1_init(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_INIT;
    printf("[%s] init.\n", task->name.c_str());
}

void sub_task_1_run(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_RUN;
    printf("[%s] run.\n", task->name.c_str());
}

void sub_task_1_done(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_DONE;
    printf("[%s] done.\n", task->name.c_str());
}

void sub_task_2_init(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_INIT;
    printf("[%s] init.\n", task->name.c_str());
}

void sub_task_2_run(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_RUN;
    printf("[%s] run.\n", task->name.c_str());
}

void sub_task_2_done(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_DONE;
    printf("[%s] done.\n", task->name.c_str());
}

void sub_task_3_init(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_INIT;
    printf("[%s] init.\n", task->name.c_str());
}

void sub_task_3_run(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_RUN;
    printf("[%s] run.\n", task->name.c_str());
}

void sub_task_3_done(void *arg)
{
    struct Task *task = (struct Task*)arg;

    task->state = TASK_STATE_DONE;
    printf("[%s] done.\n", task->name.c_str());
}

void task_config(struct Task *task, struct Task *parent_task, struct Task_info task_info)
{
    task->name = task_info.name;
    task->state = 0;
    task->parent_task = parent_task;
    task->sub_tasks.clear();
    task->task_init = task_info.task_init;
    task->task_run = task_info.task_run;
    task->task_done = task_info.task_done;
}

int main(int argc, char **argv)
{
    struct Task_info info[] = {
        { "main task", 0, 0xffffffff, main_task_init, main_task_run, main_task_done, 3, 0xfffffffe, 0xfffffffd, 0xfffffffc },
        { "sub task 1", 0x1, 0xfffffffe, sub_task_1_init, sub_task_1_run, sub_task_1_done, 0 },
        { "sub task 2", 0x2, 0xfffffffd, sub_task_2_init, sub_task_2_run, sub_task_2_done, 0 },
        { "sub task 3", 0x3, 0xfffffffc, sub_task_3_init, sub_task_3_run, sub_task_3_done, 0 },
    };
    struct Task main_task, *sub_task = NULL;
    unsigned int i;

    memset(&main_task, 0, sizeof(struct Task));
    task_config(&main_task, NULL, info[0]);

    for (i = 1; i < ARRAY_SIZE(info); i++) {
        sub_task = (struct Task*)calloc(1, sizeof(struct Task));
        if (!sub_task) {
            printf("[%s]: Allocate memory for #%d sub_task failed.\n", main_task.name.c_str(), i);
            for (i = 0; i < main_task.sub_tasks.size(); i++)
                free(main_task.sub_tasks[i]);
            main_task.sub_tasks.clear();
            return -ENOMEM;
        }
        task_config(sub_task, &main_task, info[i]);
        main_task.sub_tasks.push_back(sub_task);
        sub_task = NULL;
    }

    if (main_task.task_init)
        main_task.task_init(&main_task);
    if (main_task.task_run)
        main_task.task_run(&main_task);
    if (main_task.task_done)
        main_task.task_done(&main_task);

    for (i = 0; i < main_task.sub_tasks.size(); i++)
        free(main_task.sub_tasks[i]);

    main_task.sub_tasks.clear();

    return 0;
}
