#pragma once

#include "common.hpp"
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

enum class NodeType
{
    ROOT,
    STATIC,
    WILDCARD,
    NAMED_PARAMETER,
    REGEX_PARAMETER
};

struct Node
{
    bool is_leaf = false;
    NodeType type = NodeType::STATIC;
    std::string path;
    std::string param_name;
    std::regex pattern;
    std::unordered_map<std::string, Node*> children;
    Node* wildcard_child = nullptr;
    Node* param_child = nullptr;
    Node* regex_child = nullptr;
    RouteHandler handler;

    Node(bool is_leaf, NodeType type, const std::string& path, const RouteHandler& handler);
    inline void add_child(Node* child);
    [[nodiscard]] inline Node* get_child(const std::string& path, RouteParams& params);

    ~Node();
    void print(int depth = 0);
};

class Router
{
  public:
    Router();

    ~Router();

    void add_route(Method method, const std::string& path, const RouteHandler& handler);

    Node* find_route(Method method, const std::string& path, RouteParams& params);

    void print();

  private:
    Node* get_root = nullptr;
    Node* post_root = nullptr;
    Node* put_root = nullptr;
    Node* delete_root = nullptr;
    Node* options_root = nullptr;
    Node* patch_root = nullptr;

    std::vector<std::string> get_segments(const std::string& path);
    NodeType get_node_type(const std::string& segment);
    Node* get_root_node(Method method);
};