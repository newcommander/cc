#ifndef LEARN_H
#define LEARN_H

#include <set>
#include "node.h"
#include "task.h"

class Learn_task: public Task
{
public:
    Learn_task(std::string name, unsigned int tag,
            bool (*task_should_stop)(void *arg),
            void (*task_init)(void *arg),
            void (*task_run)(void *arg), 
            void (*task_done)(void *arg)) :
        Task(name, tag, task_should_stop, task_init, task_run, task_done) {
            type = TASK_TYPE_LEARN;
            reason.clear();
            result.clear();
        }

    std::set<Node*> reason;
    std::set<Node*> result;
};

bool learn_should_stop(void *arg);
void learn_init(void *arg);
void learn_run(void *arg);
void learn_done(void *arg);

#endif /* LEARN_H */
