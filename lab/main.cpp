#include <string>
#include <vector>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"

#define TASK_STATE_INIT 0
#define TASK_STATE_RUN 1
#define TASK_STATE_DONE 2

struct cc_task {
	const char *name;
	int state;

	struct cc_task *parent_task;
	std::vector<struct cc_task*> sub_tasks;

	void (*task_init)(void *arg);
	void (*task_run)(void *arg);
	void (*task_done)(void *arg);
};

struct cc_task_info {
	std::string name;
	void (*task_init)(void *arg);
	void (*task_run)(void *arg);
	void (*task_done)(void *arg);
};

void main_task_init(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_INIT;
	printf("[%s] init.\n", task->name);
}

void main_task_run(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;
	unsigned int i;

	task->state = TASK_STATE_RUN;
	printf("[%s] run.\n", task->name);

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
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_DONE;
	printf("[%s] done.\n", task->name);
}

void sub_task_1_init(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_INIT;
	printf("[%s] init.\n", task->name);
}

void sub_task_1_run(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_RUN;
	printf("[%s] run.\n", task->name);
}

void sub_task_1_done(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_DONE;
	printf("[%s] done.\n", task->name);
}

void sub_task_2_init(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_INIT;
	printf("[%s] init.\n", task->name);
}

void sub_task_2_run(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_RUN;
	printf("[%s] run.\n", task->name);
}

void sub_task_2_done(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_DONE;
	printf("[%s] done.\n", task->name);
}

void sub_task_3_init(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_INIT;
	printf("[%s] init.\n", task->name);
}

void sub_task_3_run(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_RUN;
	printf("[%s] run.\n", task->name);
}

void sub_task_3_done(void *arg)
{
	struct cc_task *task = (struct cc_task*)arg;

	task->state = TASK_STATE_DONE;
	printf("[%s] done.\n", task->name);
}

void cc_task_config(struct cc_task *task, struct cc_task *parent_task, struct cc_task_info task_info)
{
	task->name = task_info.name.c_str();
	task->state = 0;
	task->parent_task = parent_task;
	task->sub_tasks.clear();
	task->task_init = task_info.task_init;
	task->task_run = task_info.task_run;
	task->task_done = task_info.task_done;
}

int main(int argc, char **argv)
{
	struct cc_task_info info[] = {
		{ "main task", main_task_init, main_task_run, main_task_done },
		{ "sub task 1", sub_task_1_init, sub_task_1_run, sub_task_1_done },
		{ "sub task 2", sub_task_2_init, sub_task_2_run, sub_task_2_done },
		{ "sub task 3", sub_task_3_init, sub_task_3_run, sub_task_3_done }
	};
	struct cc_task main_task, *sub_task = NULL;
	unsigned int i;

	memset(&main_task, 0, sizeof(struct cc_task));
	cc_task_config(&main_task, NULL, info[0]);

	for (i = 1; i < ARRAY_SIZE(info); i++) {
		sub_task = (struct cc_task*)calloc(1, sizeof(struct cc_task));
		if (!sub_task) {
			printf("[%s]: Allocate memory for #%d sub_task failed.\n", main_task.name, i);
			for (i = 0; i < main_task.sub_tasks.size(); i++)
				free(main_task.sub_tasks[i]);
			main_task.sub_tasks.clear();
			return -ENOMEM;
		}
		cc_task_config(sub_task, &main_task, info[i]);
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
