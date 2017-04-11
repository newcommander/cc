#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct Task;

struct Node {
    std::string name;
    unsigned int tag;

    unsigned int task_tag;
    struct Task *task;

    std::vector<unsigned int> co_nodes_tag;
    std::vector<struct Node*> co_nodes;
};

#define TASK_STATE_INIT 0
#define TASK_STATE_RUN 1
#define TASK_STATE_DONE 2

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
