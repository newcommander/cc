#ifndef NODE_H
#define NODE_H

#include <string>
#include <set>
#include <pthread.h>
#include <json/json.h>
#include "common.h"

class Node;

#include "task.h"
#include "action.h"

class Node {
public:
    Node(std::string name, unsigned int tag) :
        name(name), tag(tag) {
        up_nodes_tags.clear();
        up_nodes.clear();
        up_nodes_shadow.clear();
        down_nodes_tags.clear();
        down_nodes.clear();
        pthread_mutex_init(&mutex, NULL);
        task = NULL;
        action = NULL;
    }

    ~Node()  {
        up_nodes_tags.clear();
        up_nodes.clear();
        up_nodes_shadow.clear();
        down_nodes_tags.clear();
        down_nodes.clear();
        pthread_mutex_destroy(&mutex);
    }

    std::string name;
    unsigned int tag;

    std::set<unsigned int> up_nodes_tags;
    std::set<Node*> up_nodes;
    std::set<unsigned int> up_nodes_shadow;
    std::set<unsigned int> down_nodes_tags;
    std::set<Node*> down_nodes;

    Task *task;
    pthread_mutex_t mutex;
    Action *action;
};

void show_all_nodes();
Node* find_node_by_tag(unsigned int tag, bool need_lock);
void reset_node_links();
void add_node(Node *node);
int add_node_by_json(Json::Value &value);
void remove_node(Node *node);
int save_all_nodes(std::string file);
int load_nodes(std::string file);
void clear_all_nodes();

#endif /* NODE_H */
