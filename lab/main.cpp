#include <vector>
#include <iostream>
#include <fstream>

#include <json/json.h>
#include "common.h"

static std::vector<Node*> g_nodes;
static std::vector<Task*> g_tasks;
static std::vector<Action*> g_actions;

int load_node(Json::Value &value)
{
    Json::Value::ArrayIndex i;
    Node *node;

    if (!value.isMember("name") ||
        !value.isMember("tag") ||
        !value.isMember("task_tag") ||
        !value.isMember("action_tag") ||
        !value.isMember("co_nodes")) {
        std::cout << "Invalid node." << std::endl;
        return -1;
    }

    node = new Node();
    if (!node)
        return -1;

    node->name = value["name"].asString();
    node->tag = value["tag"].asUInt();
    node->task_tag = value["task_tag"].asUInt();
    node->action_tag = value["action_tag"].asUInt();

    for (i = 0; i < value["co_nodes"].size(); i++)
        node->co_nodes_tags.push_back(value["co_nodes"][i].asUInt());

    g_nodes.push_back(node);

    return 0;
}

int load_task(Json::Value &value)
{
    return 0;
}

int load_action(Json::Value &value)
{
    return 0;
}

int load_data(char *file)
{
    Json::Value::ArrayIndex i;
    Json::Reader reader;
    Json::Value root;
    std::filebuf fb;

    if (fb.open(file, std::ios::in)) {
        std::istream is(&fb);
        if (!reader.parse(is, root)) {
            std::cout << "Parse json data failed." << std::endl;
            fb.close();
            return -1;
        }
        fb.close();
    } else {
        std::cout << "Open file "<< file << " failed.\n" << std::endl;
        return -1;
    }

    if (!root.isArray()) {
        std::cout << file << ": Json data in file must be an Array." << std::endl;
        return -1;
    }

    for (i = 0; i < root.size(); i++) {
        if (root[i].isMember("type")) {
            if (root[i]["type"] == "NODE") {
                if (load_node(root[i]) < 0)
                    std::cout << "load #" << i << " node failed." << std::endl;
            } else if (root[i]["type"] == "TASK") {
                if (load_task(root[i]) < 0)
                    std::cout << "load #" << i << " task failed." << std::endl;
            } else if (root[i]["type"] == "ACTION") {
                if (load_action(root[i]) < 0)
                    std::cout << "load #" << i << " action failed." << std::endl;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <data_file>" << std::endl;
        return 1;
    }

    if (load_data(argv[1]) < 0)
        return 1;

    return 0;
}
