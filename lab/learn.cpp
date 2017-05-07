#include <deque>
#include <set>
#include <stdio.h>
#include "learn.h"
#include "task.h"

void walk_on_mind(Task *task)
{
    std::deque<Node*> in_flight_nodes;
    std::set<Node*>::iterator it, down_it;
    struct Link_data *link_data;
    Node *node, *down_node;

    if ((!task) || (!task->data))
        return;
    link_data = (struct Link_data*)task->data;

    in_flight_nodes.clear();
    for (it = link_data->reason->begin(); it != link_data->reason->end(); it++)
        in_flight_nodes.push_back(*it);

    while ((task->task_should_stop ? !task->task_should_stop(task) : 0) && (in_flight_nodes.size() > 0)) {
        node = in_flight_nodes.front();
        for (down_it = node->down_nodes.begin(); down_it != node->down_nodes.end(); down_it++) {
            down_node = *down_it;
            down_node->up_nodes_shadow.insert(node->tag);
            if (down_node->up_nodes_shadow.size() >= down_node->up_nodes.size())
                in_flight_nodes.push_back(down_node);
        }
        in_flight_nodes.pop_front();
    }
}

bool learn_should_stop(void *arg)
{
    Task *task = (Task*)arg;

    if (!task)
        return true;

    return false;
}

void learn_init(void *arg)
{
    Task *task = (Task*)arg;
    struct Link_data *link_data;
    std::set<Node*>::iterator it;

    if ((!task) || (!task->data))
        return;
    link_data = (struct Link_data*)task->data;

    walk_on_mind(task);
    if (task->task_should_stop(task)) {
        task->state = TASK_STATE_WAIT_TO_RELEASE;
        return;
    }

    printf("reason: ");
    for (it = link_data->reason->begin(); it != link_data->reason->end(); it++)
        printf("%s ", (*it)->name.c_str());
    printf("\n");

    printf("result: ");
    for (it = link_data->result->begin(); it != link_data->result->end(); it++)
        printf("%s ", (*it)->name.c_str());
    printf("\n");

    printf("learn init.\n");
    task->state = TASK_STATE_RUN;
}

void learn_run(void *arg)
{
    struct Link_data *link_data;
    std::set<Node*>::iterator it;
    Task *task = (Task*)arg;
    Node *node;

    if ((!task) || (!task->data))
        return;
    link_data = (struct Link_data*)task->data;

    node = new Node("buckle", 107);
    if (!node) {
        task->state = TASK_STATE_WAIT_TO_RELEASE;
        return;
    }

    for (it = link_data->reason->begin(); it != link_data->reason->end(); it++) {
        node->up_nodes_tags.insert((*it)->tag);
        node->up_nodes.insert(*it);
        (*it)->down_nodes_tags.insert(node->tag);
        (*it)->down_nodes.insert(node);
    }
    for (it = link_data->result->begin(); it != link_data->result->end(); it++) {
        node->down_nodes_tags.insert((*it)->tag);
        node->down_nodes.insert(*it);
        (*it)->up_nodes_tags.insert(node->tag);
        (*it)->up_nodes.insert(node);
    }
    add_node(node);

    walk_on_mind(task);

    printf("learn run.\n");
    task->state = TASK_STATE_DONE;
}

void learn_done(void *arg)
{
    Task *task = (Task*)arg;

    if (!task)
        return;

    printf("learn done.\n");
    task->state = TASK_STATE_WAIT_TO_RELEASE;
}
