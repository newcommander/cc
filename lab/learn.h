#ifndef LEARN_H
#define LEARN_H

#include <set>
#include "node.h"

struct Link_data {
    std::set<Node*> *reason;
    std::set<Node*> *result;
};

bool learn_should_stop(void *arg);
void learn_init(void *arg);
void learn_run(void *arg);
void learn_done(void *arg);

#endif /* LEARN_H */
