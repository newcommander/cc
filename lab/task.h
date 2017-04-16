#ifndef TASK_H
#define TASK_H

#include <string>
#include <set>
#include <json/json.h>

class Task;

#include "node.h"

#define TASK_STATE_IDLE 0
#define TASK_STATE_INIT 1
#define TASK_STATE_RUN 2
#define TASK_STATE_DONE 3

class Task {
public:

    Task(std::string name, unsigned int tag,
            void (*task_init)(void *arg),
            void (*task_run)(void *arg), 
            void (*task_done)(void *arg)) :
        name(name), tag(tag),
        task_init(task_init),
        task_run(task_run),
        task_done(task_done) {
            state = TASK_STATE_IDLE;
            node_tag = 0;
            node = NULL;
            parent_task_tag = 0;
            parent_task = NULL;
            sub_tasks_tags.clear();
            sub_tasks.clear();
        }

    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    Node *node;

    unsigned int parent_task_tag;
    Task *parent_task;

    std::set<unsigned int> sub_tasks_tags;
    std::set<Task*> sub_tasks;

    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
};

void show_all_tasks();
int mount_all_tasks(std::string config_file);
int save_task_mount_config(std::string config_file);
void clear_all_tasks();
void load_tasks();

#endif /* TASK_H */
