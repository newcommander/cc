#include <iostream>
#include <fstream>
#include <map>

#include <json/json.h>
#include "node.h"

std::map<unsigned int, void*> g_nodes;

void show_all_nodes()
{
    std::map<unsigned int, void*>::iterator it;
    std::set<Node*>::iterator co_it;
    Node *node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = (Node*)it->second;
        std::cout << node->tag << " :";
        for (co_it = node->co_nodes.begin(); co_it != node->co_nodes.end(); co_it++)
            std::cout << " " << (*co_it)->tag;
        std::cout << std::endl;
    }
}

void reset_node_links()
{
    std::map<unsigned int, void*>::iterator it, co_it;
    std::set<unsigned int>::iterator tag_it;
    Node *node, *co_node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = (Node*)it->second;
        for (tag_it = node->co_nodes_tags.begin(); tag_it != node->co_nodes_tags.end(); tag_it++) {
            co_it = g_nodes.find(*tag_it);
            if (co_it != g_nodes.end()) {
                co_node = (Node*)co_it->second;
                if (co_node->tag != node->tag)
                    node->co_nodes.insert(co_node);
            }
        }
    }
}

int add_node(Json::Value &value)
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

void remove_node(Node *node)
{
    node->co_nodes_tags.clear();
    node->co_nodes.clear();
    delete node;
}

int save_all_nodes(std::string file)
{
    std::map<unsigned int, void*>::iterator it;
    std::set<Node*>::iterator co_it;
    Json::Value root, value, co_nodes;
    Json::FastWriter writer;
    std::ofstream outfile;
    Node *node;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        value.clear();
        node = (Node*)it->second;
        value["name"] = node->name;
        value["tag"] = node->tag;
        for (co_it = node->co_nodes.begin(); co_it != node->co_nodes.end(); co_it++)
            value["co_nodes"].append((*co_it)->tag);
        root.append(value);
    }
    outfile.open(file.c_str(), std::ofstream::out);
    outfile << writer.write(root);
    outfile.flush();
    outfile.close();

    return 0;
}

int load_nodes(std::string file)
{
    Json::Value::ArrayIndex i;
    Json::Reader reader;
    Json::Value root;
    std::filebuf fb;

    if (fb.open(file.c_str(), std::ios::in)) {
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

    for (i = 0; i < root.size(); i++)
        if (add_node(root[i]) < 0)
            std::cout << "load #" << i << " node failed." << std::endl;

    reset_node_links();

    return 0;
}

void clear_all_nodes()
{
    std::map<unsigned int, void*>::iterator it;

    for (it = g_nodes.begin(); it != g_nodes.end(); it++)
        remove_node((Node*)it->second);

    g_nodes.clear();
}
