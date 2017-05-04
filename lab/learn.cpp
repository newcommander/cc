#include <iostream>
#include <set>
#include <stdio.h>
#include "learn.h"
#include "task.h"

void walk_on_mind(Task *task)
{
	std::set<Node*> relate_nodes_1, relate_nodes_2, *p1, *p2, *tmp;
	std::set<Node*>::iterator it;
	struct Link_data *link_data;

	if ((!task) || (!task->data))
		return;
	link_data = (struct Link_data*)task->data;

	p1 = &relate_nodes_1;
	p2 = &relate_nodes_2;

	for (it = link_data->reason->begin(); it != link_data->reason->end(); it++)
		p1->insert(*it);

	while ((task->task_should_stop && task->task_should_stop(task)) || (p1->size() == 0)) {
		for (it = p1->begin(); it != p1->end(); it++) {
			;
		}
		tmp = p1;
		p1 = p2;
		p2 = tmp;
	}
}

bool learn_should_stop(void *arg)
{
	return true;
}

void learn_init(void *arg)
{
	Task *task = (Task*)arg;
	struct Link_data *link_data;
	std::set<Node*>::iterator it;

	if ((!task) || (!task->data))
		return;
	link_data = (struct Link_data*)task->data;

	printf("reason: ");
	for (it = link_data->reason->begin(); it != link_data->reason->end(); it++)
		printf("%s ", (*it)->name.c_str());
	printf("\n");

	printf("result: ");
	for (it = link_data->result->begin(); it != link_data->result->end(); it++)
		printf("%s ", (*it)->name.c_str());
	printf("\n");

	std::cout << "learn init" << std::endl;
	task->state = TASK_STATE_RUN;
}

void learn_run(void *arg)
{
	Task *task = (Task*)arg;
	struct Link_data *link_data;
	std::set<Node*>::iterator it;

	if ((!task) || (!task->data))
		return;
	link_data = (struct Link_data*)task->data;

	std::cout << "learn run" << std::endl;
	task->state = TASK_STATE_DONE;
}

void learn_done(void *arg)
{
	Task *task = (Task*)arg;

	if (!task)
		return;

	std::cout << "learn done" << std::endl;
	task->state = TASK_STATE_WAIT_TO_RELEASE;
}
