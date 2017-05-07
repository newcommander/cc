#ifndef TIMING_H
#define TIMING_H

#include "node.h"

#define TIMING_CAPECITY 10 // seconds

int add_node_to_timer(Node *node, unsigned int index);
int start_timing_wheel();
void stop_timing_wheel();

#endif /* TIMING_H */
