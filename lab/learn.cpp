#include <sstream>
#include <deque>
#include <set>
#include <stdio.h>
#include "learn.h"
#include "task.h"
#include "timing.h"

bool learn_should_stop(void *arg)
{
    Learn_task *learn = (Learn_task*)arg;

    if (!learn)
        return true;

    return false;
}

void learn_init(void *arg)
{
    Learn_task *learn = (Learn_task*)arg;
    std::set<Node*>::iterator it, co_it;

    if (!learn)
        return;

//    walk_on_mind(learn);
    if (learn->task_should_stop(learn)) {
        learn->state = TASK_STATE_WAIT_TO_RELEASE;
        return;
    }

    printf("reason: ");
    for (it = learn->reason.begin(); it != learn->reason.end(); it++)
        printf("%s ", (*it)->name.c_str());
    printf(" --> ");

    printf("result: ");
    for (it = learn->result.begin(); it != learn->result.end(); it++)
        printf("%s ", (*it)->name.c_str());
    printf("\n");

    learn->state = TASK_STATE_RUN;
}

static std::string int_to_string(int i)
{
    std::stringstream stream;
    stream << i;
    return stream.str();
}

int lala = 0;
void learn_run(void *arg)
{
    std::set<Node*>::iterator it;
    Learn_task *learn = (Learn_task*)arg;
    Node *node;

    if (!learn)
        return;

    node = new Node("buckle" + int_to_string(lala), 107 + lala);
    if (!node) {
        learn->state = TASK_STATE_WAIT_TO_RELEASE;
        return;
    }
    lala++;

    for (it = learn->reason.begin(); it != learn->reason.end(); it++) {
        node->up_nodes_tags.insert((*it)->tag);
        node->up_nodes.insert(*it);
        (*it)->down_nodes_tags.insert(node->tag);
        (*it)->down_nodes.insert(node);
    }
    for (it = learn->result.begin(); it != learn->result.end(); it++) {
        node->down_nodes_tags.insert((*it)->tag);
        node->down_nodes.insert(*it);
        (*it)->up_nodes_tags.insert(node->tag);
        (*it)->up_nodes.insert(node);
    }
    add_node(node);

//    walk_on_mind(learn);

    learn->state = TASK_STATE_DONE;
}

void learn_done(void *arg)
{
    Learn_task *learn = (Learn_task*)arg;

    if (!learn)
        return;

    learn->state = TASK_STATE_WAIT_TO_RELEASE;
}
