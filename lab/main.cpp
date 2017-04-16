#include <iostream>
#include "node.h"

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

    clear_all_actions();
mount_all_actions_failed:
    clear_all_tasks();
mount_all_tasks_failed:
    clear_all_nodes();
load_nodes_failed:

    return ret;
}
