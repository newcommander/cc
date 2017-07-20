#ifndef ACTION_H
#define ACTION_H

#include <string>
#include <json/json.h>

class Action;

#include "node.h"

#define ACTION_STATE_IDLE 0
#define ACTION_STATE_INIT 1
#define ACTION_STATE_RUN 2
#define ACTION_STATE_DONE 3

class Action {
public:

    Action(std::string name, unsigned int tag,
            void (*action_init)(void *arg),
            void (*action_run)(void *arg), 
            void (*action_done)(void *arg)) :
        name(name), tag(tag),
        action_init(action_init),
        action_run(action_run),
        action_done(action_done) {
            state = ACTION_STATE_IDLE;
            node_tag = 0;
            node = NULL;
        }

    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    Node *node;

    void (*action_init)(void *arg);
    void (*action_run)(void *arg);
    void (*action_done)(void *arg);
};

void show_all_actions();
int mount_all_actions(std::string config_file);
int save_action_mount_config(std::string config_file);
void clear_all_actions();
void load_actions();

#endif /* ACTION_H */
