#include <iostream>
#include <fstream>
#include "task.h"

std::map<unsigned int, void*> g_tasks;

extern Task task_1;
extern Task task_2;
extern Task task_3;
extern Task task_4;

static Task *online_tasks[] = {
    &task_1,
    &task_2,
    &task_3,
    &task_4,
};

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
    std::map<unsigned int, void*>::iterator task_it, node_it, sub_task_it;
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

    node_it = g_nodes.find(node_tag);
    if (node_it == g_nodes.end()) {
        std::cout << "Cannot find node(tag=" << node_tag << ") for task(tag=" << task_tag << ")" << std::endl;
        return -1;
    }
    node = (Node*)node_it->second;

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

static void add_task(Task *task)
{
    g_tasks.insert(KV(task->tag, task));
}

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
        task->sub_tasks_tags.clear();
        task->sub_tasks.clear();
    }
    g_tasks.clear();
}

void load_tasks()
{
    size_t i;

    g_tasks.clear();
    for (i = 0; i < ARRAY_SIZE(online_tasks); i++)
        add_task(online_tasks[i]);
}