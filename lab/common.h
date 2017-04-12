#include <string>

class Task;
class Action;

class Node {
public:
    std::string name;
    unsigned int tag;

    unsigned int task_tag;
    Task *task;

    unsigned int action_tag;
    Action *action;

    std::vector<unsigned int> co_nodes_tags;
    std::vector<Node*> co_nodes;
};

#define TASK_STATE_IDLE 0
#define TASK_STATE_INIT 1
#define TASK_STATE_RUN 2
#define TASK_STATE_DONE 3

class Task {
public:
    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    Node *node;

    unsigned int parent_task_tag;
    Task *parent_task;

    std::vector<unsigned int> sub_tasks_tags;
    std::vector<Task*> sub_tasks;

    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
};

#define ACTION_STATE_IDLE 0
#define ACTION_STATE_INIT 1
#define ACTION_STATE_RUN 2
#define ACTION_STATE_DONE 3

class Action {
public:
    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    Node *node;

    void (*action_init)(void *arg);
    void (*action_run)(void *arg);
    void (*action_done)(void *arg);
};
