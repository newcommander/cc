#ifndef NODE_H
#define NODE_H

#include <string>
#include <set>
#include <json/json.h>
#include "common.h"

class Node;

#include "task.h"
#include "action.h"

class Node {
public:
    std::string name;
    unsigned int tag;

    Task *task;
    Action *action;

    std::set<unsigned int> co_nodes_tags;
    std::set<Node*> co_nodes;
};

void show_all_nodes();
void reset_node_links();
int add_node(Json::Value &value);
void remove_node(Node *node);
int save_all_nodes(std::string file);
int load_nodes(std::string file);
void clear_all_nodes();

extern std::map<unsigned int, void*> g_nodes;

#endif /* NODE_H */
