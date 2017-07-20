#include <deque>
#include <set>
#include "node.h"
#include "task.h"
#include "timing.h"

void walk_on_mind(Task *task)
{
    std::deque<Node*> in_flight_nodes;
    std::set<Node*>::iterator it, down_it;
    Node *node, *down_node;

    if (!task)
        return;

    in_flight_nodes.clear();
    for (it = task->begin_nodes.begin(); it != task->begin_nodes.end(); it++)
        in_flight_nodes.push_back(*it);

    while ((task->task_should_stop ? !task->task_should_stop(task) : true) && (in_flight_nodes.size() > 0)) {
        node = in_flight_nodes.front();
        for (down_it = node->down_nodes.begin(); down_it != node->down_nodes.end(); down_it++) {
            down_node = *down_it;
            down_node->up_nodes_shadow.insert(node->tag);
            if (!down_node->in_timer)
                add_node_to_timer(down_node);
            if (down_node->up_nodes_shadow.size() >= down_node->up_nodes.size()) {
                del_node_from_timer(down_node);
                down_node->up_nodes_shadow.clear();
                in_flight_nodes.push_back(down_node);
            }
        }
        in_flight_nodes.pop_front();
    }
}
