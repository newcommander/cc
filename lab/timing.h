#ifndef TIMING_H
#define TIMING_H

#include "node.h"

#define TIMING_CAPECITY 10

void add_node_to_timer(Node *node);
void del_node_from_timer(Node *node);
int start_timing_wheel();
void stop_timing_wheel();

#endif /* TIMING_H */
