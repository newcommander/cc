#include <vector>
#include <set>
#include <pthread.h>
#include <stdio.h>
#include "common.h"
#include "timing.h"

static pthread_t timer_thread;
static std::vector<std::set<Node*> > timer;
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;

int add_node_to_timer(Node *node, unsigned int index)
{
    int i;

    if ((!node) || (index >= timer.size()))
        return -1;

    pthread_mutex_lock(&timer_mutex);
    for (i = 0; i < TIMING_CAPECITY; i++)
        timer[i].erase(node);
    timer[index].insert(node);
    pthread_mutex_unlock(&timer_mutex);

    return 0;
}

static void* main_loop(void *arg)
{
    std::set<Node*>::iterator it;
    std::set<Node*> timer_node;
    int i;

    timer.clear();
    for (i = 0; i < TIMING_CAPECITY; i++) {
        timer_node.clear();
        timer.push_back(timer_node);
    }
    // TODO: when to clear timer ?

    i = 0;
    while (1) {
        msleep(1000);
        pthread_mutex_lock(&timer_mutex);
        for (it = timer[i].begin(); it != timer[i].end(); it++) {
            (*it)->up_nodes_shadow.clear();
        }
        timer[i].clear();
        i = (i + 1) % timer.size();
        pthread_mutex_unlock(&timer_mutex);
    }

    return NULL;
}

int start_timing_wheel()
{
    if (pthread_create(&timer_thread, NULL, main_loop, NULL) != 0) {
        printf("Creating timing_wheel thread failed.\n");
        return -1;
    }
    return 0;
}

void stop_timing_wheel()
{
    pthread_cancel(timer_thread);
    pthread_join(timer_thread, NULL);
}
