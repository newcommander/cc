#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "node.h"
#include "learn.h"

static std::set<Node*> reason;
static std::set<Node*> result;

struct Link_data link_data = {
    &reason, &result
};

void work()
{
    Task *task = new Task("learning task", 0x5, learn_should_stop, learn_init, learn_run, learn_done);
    reason.insert(find_node_by_tag(6)); // 5
    reason.insert(find_node_by_tag(8)); // 7
    reason.insert(find_node_by_tag(102)); // +
    result.insert(find_node_by_tag(13)); // 12
    link_data.reason = &reason;
    link_data.result = &result;
    task->data = &link_data;
    add_task(task);
    add_running_task(task);
}

int main(int argc, char **argv)
{
    int ret = 0;

    if (load_nodes("node.json") < 0) {
        ret = 1;
        goto load_nodes_failed;
    }
//    show_all_nodes();

    load_tasks();
    if (mount_all_tasks("task.json") < 0) {
        ret = 1;
        goto mount_all_tasks_failed;
    }
//    show_all_tasks();

    load_actions();
    if (mount_all_actions("action.json") < 0) {
        ret = 1;
        goto mount_all_actions_failed;
    }
//    show_all_actions();

    start_task_launcher();
    work();
    sleep(1);
    stop_task_launcher();

    save_all_nodes("new.json");

    clear_all_actions();
mount_all_actions_failed:
    clear_all_tasks();
mount_all_tasks_failed:
    clear_all_nodes();
load_nodes_failed:

    return ret;
}
