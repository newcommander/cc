#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct Task;
struct Action;

struct Node {
    std::string name;
    unsigned int tag;

    unsigned int task_tag;
    struct Task *task;

    unsigned int action_tag;
    struct Action *action;

    std::vector<unsigned int> co_nodes_tag;
    std::vector<struct Node*> co_nodes;
};

struct Node_info {
    std::string name;
    unsigned int tag;
    unsigned int task_tag;
    unsigned int action_tag;
    size_t co_node_num;
    unsigned int co_nodes[];
};

#define TASK_STATE_IDLE 0
#define TASK_STATE_INIT 1
#define TASK_STATE_RUN 2
#define TASK_STATE_DONE 3

struct Task {
    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    struct Node *node;

    unsigned int parent_task_tag;
    struct Task *parent_task;

    std::vector<unsigned int> sub_tasks_tag;
    std::vector<struct Task*> sub_tasks;

    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
};

struct Task_info {
    std::string name;
    unsigned int tag;
    unsigned int node_tag;
    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
    size_t sub_task_num;
    unsigned int sub_tasks_tag[];
};

#define ACTION_STATE_IDLE 0
#define ACTION_STATE_INIT 1
#define ACTION_STATE_RUN 2
#define ACTION_STATE_DONE 3

struct Action {
    std::string name;
    unsigned int tag;
    int state;

    unsigned int node_tag;
    struct Node *node;

    void (*action_init)(void *arg);
    void (*action_run)(void *arg);
    void (*action_done)(void *arg);
};

struct Action_info {
    std::string name;
    unsigned int tag;
    unsigned int node_tag;
    void (*task_init)(void *arg);
    void (*task_run)(void *arg);
    void (*task_done)(void *arg);
};
