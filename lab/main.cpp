#include <iostream>
#include <unistd.h>
#include "node.h"

void work()
{
    int i = 5;
    Task *task = new Task("task_5", 0x5, NULL, NULL, NULL);
    add_running_task(task);
    del_running_task(task, true);
    while (i--)
        sleep(1);
}

int main(int argc, char **argv)
{
    int ret = 0;

    if (load_nodes("node.json") < 0) {
        ret = 1;
        goto load_nodes_failed;
    }
    show_all_nodes();

    load_tasks();
    if (mount_all_tasks("task.json") < 0) {
        ret = 1;
        goto mount_all_tasks_failed;
    }
    show_all_tasks();

    load_actions();
    if (mount_all_actions("action.json") < 0) {
        ret = 1;
        goto mount_all_actions_failed;
    }
    show_all_actions();

    start_task_launcher();
    work();
    stop_task_launcher();

    clear_all_actions();
mount_all_actions_failed:
    clear_all_tasks();
mount_all_tasks_failed:
    clear_all_nodes();
load_nodes_failed:

    return ret;
}
