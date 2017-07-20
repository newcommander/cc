#include <iostream>
#include <fstream>
#include <map>
#include <pthread.h>
#include <json/json.h>
#include "node.h"

static std::map<unsigned int, void*> g_nodes;
static pthread_mutex_t g_nodes_mutex = PTHREAD_MUTEX_INITIALIZER;

void show_all_nodes()
{
    std::map<unsigned int, void*>::iterator it;
    std::set<Node*>::iterator co_it;
    Node *node;

    pthread_mutex_lock(&g_nodes_mutex);
    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = (Node*)it->second;
        std::cout << node->tag << " :";
        for (co_it = node->down_nodes.begin(); co_it != node->down_nodes.end(); co_it++)
            std::cout << " " << (*co_it)->tag;
        std::cout << std::endl;
    }
    pthread_mutex_unlock(&g_nodes_mutex);
}

Node* find_node_by_tag(unsigned int tag, bool need_lock)
{
    std::map<unsigned int, void*>::iterator it;
    Node *node;

    if (tag == 0)
        return NULL;

    if (need_lock)
        pthread_mutex_lock(&g_nodes_mutex);

    it = g_nodes.find(tag);
    if (it == g_nodes.end())
        node = NULL;
    else
        node = (Node*)it->second;

    if (need_lock)
        pthread_mutex_unlock(&g_nodes_mutex);

    return node;
}

void reset_node_links()
{
    std::map<unsigned int, void*>::iterator it;
    std::set<unsigned int>::iterator tag_it;
    Node *node, *co_node;

    pthread_mutex_lock(&g_nodes_mutex);
    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        node = (Node*)it->second;
        node->down_nodes.clear();
        for (tag_it = node->down_nodes_tags.begin(); tag_it != node->down_nodes_tags.end(); tag_it++) {
            co_node = find_node_by_tag(*tag_it, false);
            if (co_node && (co_node->tag != node->tag))
                node->down_nodes.insert(co_node);
        }
        node->up_nodes.clear();
        for (tag_it = node->up_nodes_tags.begin(); tag_it != node->up_nodes_tags.end(); tag_it++) {
            co_node = find_node_by_tag(*tag_it, false);
            if (co_node && (co_node->tag != node->tag))
                node->up_nodes.insert(co_node);
        }
    }
    pthread_mutex_unlock(&g_nodes_mutex);
}

void add_node(Node *node)
{
    if (!node)
        return;
    pthread_mutex_lock(&g_nodes_mutex);
    g_nodes.insert(KV(node->tag, node));
    pthread_mutex_unlock(&g_nodes_mutex);
}

int add_node_by_json(Json::Value &value)
{
    Json::Value::ArrayIndex i;
    Node *node;

    if (!value.isMember("name") || !value["name"].isString() ||
        !value.isMember("tag") || !value["tag"].isUInt()) {
        std::cout << "Invalid node." << std::endl;
        return -1;
    }

    node = new Node(value["name"].asString(), value["tag"].asUInt());
    if (!node)
        return -1;

    if (value.isMember("down_nodes") && value["down_nodes"].isArray()) {
        for (i = 0; i < value["down_nodes"].size(); i++)
            node->down_nodes_tags.insert(value["down_nodes"][i].asUInt());
    }

    if (value.isMember("up_nodes") && value["up_nodes"].isArray()) {
        for (i = 0; i < value["up_nodes"].size(); i++)
            node->up_nodes_tags.insert(value["up_nodes"][i].asUInt());
    }

    add_node(node);

    return 0;
}

void remove_node(Node *node)
{
    if (!node)
        return;

    delete node;
}

int save_all_nodes(std::string file)
{
    std::map<unsigned int, void*>::iterator it;
    std::set<Node*>::iterator co_it;
    Json::Value root, value, down_nodes;
    Json::FastWriter writer;
    std::ofstream outfile;
    Node *node;

    pthread_mutex_lock(&g_nodes_mutex);
    for (it = g_nodes.begin(); it != g_nodes.end(); it++) {
        value.clear();
        node = (Node*)it->second;
        value["name"] = node->name;
        value["tag"] = node->tag;
        for (co_it = node->down_nodes.begin(); co_it != node->down_nodes.end(); co_it++)
            value["down_nodes"].append((*co_it)->tag);
        for (co_it = node->up_nodes.begin(); co_it != node->up_nodes.end(); co_it++)
            value["up_nodes"].append((*co_it)->tag);
        root.append(value);
    }
    pthread_mutex_unlock(&g_nodes_mutex);
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
        std::cout << file << ": Json data must be in the format of Array." << std::endl;
        return -1;
    }

    for (i = 0; i < root.size(); i++)
        if (add_node_by_json(root[i]) < 0)
            std::cout << "load #" << i << " node failed." << std::endl;

    reset_node_links();

    return 0;
}

void clear_all_nodes()
{
    std::map<unsigned int, void*>::iterator it;

    pthread_mutex_lock(&g_nodes_mutex);
    for (it = g_nodes.begin(); it != g_nodes.end(); it++)
        remove_node((Node*)it->second);

    g_nodes.clear();
    pthread_mutex_unlock(&g_nodes_mutex);
}
