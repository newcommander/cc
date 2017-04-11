#include <string>
#include <vector>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"

static std::vector<struct Node*> g_nodes;
static std::vector<struct Task*> g_tasks;
static std::vector<struct Action*> g_actions;

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
    unsigned int i;

    task->name = task_info.name;
    task->tag = task_info.tag;
    task->state = TASK_STATE_IDLE;
    task->node_tag = task_info.node_tag;
    task->node = NULL;
    if (parent_task)
        task->parent_task_tag = parent_task->tag;
    else
        task->parent_task_tag = 0;
    task->parent_task = NULL;
    for (i = 0; i < task_info.sub_task_num; i++)
        task->sub_tasks_tag.push_back(task_info.sub_tasks_tag[i]);
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
    struct Task *task = NULL;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(info); i++) {
        task = (struct Task*)calloc(1, sizeof(struct Task));
        if (!task) {
            printf("Allocate memory for #%d task failed.\n", i);
            for (i = 0; i < main_task.sub_tasks.size(); i++)
                free(main_task.sub_tasks[i]);
            main_task.sub_tasks.clear();
            return -ENOMEM;
        }
        task_config(task, &main_task, info[i]);
        g_tasks.push_back(task);
        task = NULL;
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
