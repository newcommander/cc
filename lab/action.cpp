#include <iostream>
#include <fstream>
#include "action.h"

std::map<unsigned int, void*> g_actions;

extern Action action_1;
extern Action action_2;
extern Action action_3;
extern Action action_4;

static Action *online_actions[] = {
    &action_1,
    &action_2,
    &action_3,
    &action_4,
};

void show_all_actions()
{
    std::map<unsigned int, void*>::iterator it;
    Action *action;

    for (it = g_actions.begin(); it != g_actions.end(); it++) {
        action = (Action*)it->second;
        std::cout << action->name << "[node=" << action->node_tag << "]" << std::endl;
    }
}

static int mount_action(Json::Value &value)
{
    std::map<unsigned int, void*>::iterator action_it;
    Action *action;
    Node *node;
    unsigned int action_tag, node_tag;

    if (!value.isMember("tag") || !value["tag"].isUInt() ||
        !value.isMember("node_tag") || !value["node_tag"].isUInt()) {
        std::cout << "Invalid action." << std::endl;
        return -1;
    }

    action_tag = value["tag"].asUInt();
    node_tag = value["node_tag"].asUInt();

    action_it = g_actions.find(action_tag);
    if (action_it == g_actions.end()) {
        std::cout << "Cannot find action(tag=" << action_tag << ")" << std::endl;
        return -1;
    }
    action = (Action*)action_it->second;

    node = find_node_by_tag(node_tag, true);
    if (!node) {
        std::cout << "Cannot find node(tag=" << node_tag << ") for action(tag=" << action_tag << ")" << std::endl;
        return -1;
    }

    action->node_tag = node_tag;
    action->node = node;
    node->action = action;

    return 0;
}

int mount_all_actions(std::string config_file)
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
        if (mount_action(root[i]) < 0)
            std::cout << "mount #" << i << " action failed." << std::endl;
    }

    return 0;
}

static void add_action(Action *action)
{
    g_actions.insert(KV(action->tag, action));
}

int save_action_mount_config(std::string config_file)
{
    std::map<unsigned int, void*>::iterator it;
    Json::Value root, value;
    Json::FastWriter writer;
    std::ofstream outfile;
    Action *action;

    for (it = g_actions.begin(); it != g_actions.end(); it++) {
        value.clear();
        action = (Action*)it->second;
        value["tag"] = action->tag;
        value["node_tag"] = action->node_tag;
        root.append(value);
    }
    outfile.open(config_file.c_str(), std::ofstream::out);
    outfile << writer.write(root);
    outfile.flush();
    outfile.close();

    return 0;
}

void clear_all_actions()
{
    g_actions.clear();
}

void load_actions()
{
    size_t i;

    g_actions.clear();
    for (i = 0; i < ARRAY_SIZE(online_actions); i++)
        add_action(online_actions[i]);
}
