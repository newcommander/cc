#ifndef TASK_H
#define TASK_H

#include <string>
#include <set>
#include <pthread.h>
#include <json/json.h>

class Task;

#include "node.h"

#define TASK_STATE_WAIT_TO_LAUNCH 0
#define TASK_STATE_INIT 1
#define TASK_STATE_RUN 2
#define TASK_STATE_SLEEP 3
#define TASK_STATE_DONE 4
#define TASK_STATE_WAIT_TO_RELEASE 5

#define TASK_TYPE_NORMAL 0
#define TASK_TYPE_LEARN  1

class Task {
public:

    Task(std::string name, unsigned int tag,
            bool (*task_should_stop)(void *arg),
            void (*task_init)(void *arg),
            void (*task_run)(void *arg), 
            void (*task_done)(void *arg)) :
        name(name), tag(tag),
        task_should_stop(task_should_stop),
        task_init(task_init),
        task_run(task_run),
        task_done(task_done) {
            type = TASK_TYPE_NORMAL;
            state = TASK_STATE_WAIT_TO_LAUNCH;
            data = NULL;
            begin_nodes.clear();
        }

    std::string name;
    unsigned int tag;
    int type;
    int state;
    void *data;

    std::set<Node*> begin_nodes;

    bool (*task_should_stop)(void *arg);
    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
};

/*
void show_all_tasks();
int mount_all_tasks(std::string config_file);
int save_task_mount_config(std::string config_file);
void clear_all_tasks();
void load_tasks();
*/
void add_task(Task *task);
void del_task(Task *task);

void add_running_task(Task *task);
void del_running_task(Task *task, bool need_lock);
int start_task_launcher();
void stop_task_launcher();

#endif /* TASK_H */
