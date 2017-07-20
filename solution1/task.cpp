#include <iostream>
#include <fstream>
#include <deque>
#include "task.h"

static std::map<unsigned int, void*> g_tasks;
static std::deque<Task*> running_tasks;
static pthread_mutex_t running_tasks_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t task_launcher, task_running;

/*
void show_all_tasks()
{
    std::map<unsigned int, void*>::iterator it;
    std::set<Task*>::iterator sub_it;
    Task *task;

    for (it = g_tasks.begin(); it != g_tasks.end(); it++) {
        task = (Task*)it->second;
        std::cout << task->name << "[node=" << task->node_tag << "] sub_tasks :";
        for (sub_it = task->sub_tasks.begin(); sub_it != task->sub_tasks.end(); sub_it++)
            std::cout << " " << (*sub_it)->name;
        std::cout << std::endl;
    }
}

static int mount_task(Json::Value &value)
{
    Json::Value::ArrayIndex i;
    std::map<unsigned int, void*>::iterator task_it, sub_task_it;
    Task *task, *sub_task;
    Node *node;
    unsigned int task_tag, node_tag;

    if (!value.isMember("tag") || !value["tag"].isUInt() ||
        !value.isMember("node_tag") || !value["node_tag"].isUInt() ||
        !value.isMember("sub_tasks") || !value["sub_tasks"].isArray()) {
        std::cout << "Invalid task." << std::endl;
        return -1;
    }

    task_tag = value["tag"].asUInt();
    node_tag = value["node_tag"].asUInt();

    task_it = g_tasks.find(task_tag);
    if (task_it == g_tasks.end()) {
        std::cout << "Cannot find task(tag=" << task_tag << ")" << std::endl;
        return -1;
    }
    task = (Task*)task_it->second;

    node = find_node_by_tag(node_tag);
    if (!node) {
        std::cout << "Cannot find node(tag=" << node_tag << ") for task(tag=" << task_tag << ")" << std::endl;
        return -1;
    }

    task->node_tag = node_tag;
    task->node = node;
    node->task = task;

    for (i = 0; i < value["sub_tasks"].size(); i++) {
        sub_task_it = g_tasks.find(value["sub_tasks"][i].asUInt());
        if (sub_task_it == g_tasks.end()) {
            std::cout << "Cannon find sub_task(tag=" << value["sub_tasks"][i].asUInt() << ") for task(tag=" << task_tag << ")" << std::endl;
            continue;
        }
        sub_task = (Task*)sub_task_it->second;
        if (sub_task->tag == task->tag)
            continue;
        task->sub_tasks_tags.insert(sub_task->tag);
        task->sub_tasks.insert(sub_task);
    }

    return 0;
}

int mount_all_tasks(std::string config_file)
{
    Json::Value::ArrayIndex i;
    Json::Reader reader;
    Json::Value root;
    std::filebuf fb;

    if (fb.open(config_file.c_str(), std::ios::in)) {
        std::istream is(&fb);
        if (!reader.parse(is, root)) {
            std::cout << "Parse json data failed." << std::endl;
            fb.close();
            return -1;
        }
        fb.close();
    } else {
        std::cout << "Open file "<< config_file << " failed.\n" << std::endl;
        return -1;
    }

    if (!root.isArray()) {
        std::cout << config_file << ": Json data in file must be an Array." << std::endl;
        return -1;
    }

    for (i = 0; i < root.size(); i++) {
        if (mount_task(root[i]) < 0)
            std::cout << "mount #" << i << " task failed." << std::endl;
    }

    return 0;
}
*/

void add_task(Task *task)
{
    if (!task)
        return;
    g_tasks.insert(KV(task->tag, task));
}

void del_task(Task *task)
{
    if (!task || (task->tag == 0))
        return;
    delete task;
    g_tasks.erase(task->tag);
}

/*
int save_task_mount_config(std::string config_file)
{
    std::map<unsigned int, void*>::iterator it;
    std::set<unsigned int>::iterator sub_it;
    Json::Value root, value;
    Json::FastWriter writer;
    std::ofstream outfile;
    Task *task;

    for (it = g_tasks.begin(); it != g_tasks.end(); it++) {
        value.clear();
        task = (Task*)it->second;
        value["tag"] = task->tag;
        value["node_tag"] = task->node_tag;
        for (sub_it = task->sub_tasks_tags.begin(); sub_it != task->sub_tasks_tags.end(); sub_it++)
            value["sub_tasks"].append(*sub_it);
        root.append(value);
    }
    outfile.open(config_file.c_str(), std::ofstream::out);
    outfile << writer.write(root);
    outfile.flush();
    outfile.close();

    return 0;
}

void clear_all_tasks()
{
    std::map<unsigned int, void*>::iterator it;
    Task *task;

    for (it = g_tasks.begin(); it != g_tasks.end(); it++) {
        task = (Task*)it->second;
        delete task;
    }
    g_tasks.clear();
}

void load_tasks()
{
    g_tasks.clear();
}
*/

static void* run_task(void *arg)
{
    Task *task;

    while (1) {
        pthread_mutex_lock(&running_tasks_mutex);
        if (running_tasks.size() == 0) {
            pthread_mutex_unlock(&running_tasks_mutex);
            msleep(50);
            continue;
        }
        task = running_tasks.front();
        running_tasks.pop_front();
        pthread_mutex_unlock(&running_tasks_mutex);

        if (!task)
            continue;

        task->state = TASK_STATE_INIT;
        while (task->state != TASK_STATE_WAIT_TO_RELEASE) {
            switch (task->state) {
            case TASK_STATE_INIT:
                if (task->task_init)
                    task->task_init(task);
                else
                    task->state = TASK_STATE_RUN;
                break;
            case TASK_STATE_RUN:
                if (task->task_run)
                    task->task_run(task);
                else
                    task->state = TASK_STATE_DONE;
                break;
            case TASK_STATE_DONE:
                if (task->task_done)
                    task->task_done(task);
                else
                    task->state = TASK_STATE_WAIT_TO_RELEASE;
                break;
            }
        }
    }

    return NULL;
}

static void* task_launcher_func(void *arg)
{
    std::map<unsigned int, void*>::iterator it;
    Task *task = NULL;

    if (pthread_create(&task_running, NULL, run_task, NULL) != 0) {
        std::cout << "Creating task(" << task->name << ") thread failed." << std::endl;
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&running_tasks_mutex);
        for (it = g_tasks.begin(); it != g_tasks.end(); it++) {
            task = (Task*)it->second;
            switch (task->state) {
            case TASK_STATE_WAIT_TO_LAUNCH:
                running_tasks.push_back(task);
                break;
            case TASK_STATE_WAIT_TO_RELEASE:
//                running_tasks.erase(task);
                break;
            }
        }
        pthread_mutex_unlock(&running_tasks_mutex);
        msleep(200);
    }

    return NULL;
}

int start_task_launcher()
{
    if (pthread_create(&task_launcher, NULL, task_launcher_func, NULL) != 0) {
        std::cout << "Creating task_launcher thread failed." << std::endl;
        return -1;
    }
    return 0;
}

void stop_task_launcher()
{
    pthread_cancel(task_launcher);
    pthread_join(task_launcher, NULL);
}
