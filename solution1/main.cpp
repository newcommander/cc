#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "node.h"
#include "learn.h"

int task_lala = 5;
void work()
{
    int i, j;

    for (i = 0; i < 10; i++) {
        for (j = 0; j < 10; j++) {
            if (i == j)
                continue;
            Learn_task *learn = new Learn_task("learning task", task_lala++, learn_should_stop, learn_init, learn_run, learn_done);
            learn->reason.insert(find_node_by_tag(i+1, true)); // 5
            learn->reason.insert(find_node_by_tag(j+1, true)); // 7
            learn->reason.insert(find_node_by_tag(102, true)); // +
            learn->result.insert(find_node_by_tag(i+j+1, true)); // 12
            add_task(learn);
        }
    }
}

int main(int argc, char **argv)
{
    int ret = 0;

    if (load_nodes("node.json") < 0) {
        ret = 1;
        goto load_nodes_failed;
    }
//    show_all_nodes();

/*
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
*/

    start_task_launcher();
    work();
    sleep(5);
    stop_task_launcher();

    save_all_nodes("new.json");

//    clear_all_actions();
//mount_all_actions_failed:
//    clear_all_tasks();
//mount_all_tasks_failed:
    clear_all_nodes();
load_nodes_failed:

    return ret;
}
