#include <map>
#include <iostream>
#include <fstream>

#include <json/json.h>
#include "common.h"

typedef std::pair<unsigned int, Node*> KV;

static std::map<unsigned int, Node*> g_nodes;

void show_nodes()
{
    std::map<unsigned int, Node*>::iterator it;
    std::set<Node*>::iterator co_it;
    Node *node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = it->second;
        std::cout << node->tag << " :";
        for (co_it = node->co_nodes.begin(); co_it != node->co_nodes.end(); co_it++)
            std::cout << " " << (*co_it)->tag;
        std::cout << std::endl;
    }
}

void reset_node_links()
{
    std::map<unsigned int, Node*>::iterator it, co_it;
    std::set<unsigned int>::iterator tag_it;
    Node *node, *co_node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = it->second;
        for (tag_it = node->co_nodes_tags.begin(); tag_it != node->co_nodes_tags.end(); tag_it++) {
            co_it = g_nodes.find(*tag_it);
            if (co_it != g_nodes.end()) {
                co_node = co_it->second;
                if (co_node->tag != node->tag)
                    node->co_nodes.insert(co_node);
            }
        }
    }
}

int load_node(Json::Value &value)
{
    Json::Value::ArrayIndex i;
    Node *node;

    if (!value.isMember("name") || !value["name"].isString() ||
        !value.isMember("tag") || !value["tag"].isUInt() ||
        !value.isMember("co_nodes") || !value["co_nodes"].isArray()) {
        std::cout << "Invalid node." << std::endl;
        return -1;
    }

    node = new Node();
    if (!node)
        return -1;

    node->name = value["name"].asString();
    node->tag = value["tag"].asUInt();
    node->task = NULL;
    node->action = NULL;
    node->co_nodes_tags.clear();
    node->co_nodes.clear();

    for (i = 0; i < value["co_nodes"].size(); i++)
        node->co_nodes_tags.insert(value["co_nodes"][i].asUInt());

    g_nodes.insert(KV(node->tag, node));

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

int save_data(char *file)
{
    std::map<unsigned int, Node*>::iterator it;
    std::set<Node*>::iterator co_it;
    Json::Value root, value, co_nodes;
    Json::FastWriter writer;
    std::ofstream outfile;
    Node *node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = it->second;
        value["type"] = "NODE";
        value["name"] = node->name;
        value["tag"] = node->tag;
        for (co_it = node->co_nodes.begin(); co_it != node->co_nodes.end(); co_it++)
            value["co_nodes"].append((*co_it)->tag);
        root.append(value);
        value.clear();
    }
    outfile.open("out.json", std::ofstream::out);
    outfile << writer.write(root);
    outfile.flush();
    outfile.close();
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

    reset_node_links();
    show_nodes();
    save_data(NULL);

    return 0;
}

int main(int argc, char **argv)
{
    std::map<unsigned int, Node*>::iterator it;
    Node *node;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <data_file>" << std::endl;
        return 1;
    }

    if (load_data(argv[1]) < 0)
        return 1;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = it->second;
        node->co_nodes_tags.clear();
        node->co_nodes.clear();
        delete node;
    }
    g_nodes.clear();

    return 0;
}
