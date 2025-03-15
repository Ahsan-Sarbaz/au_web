#include "router.hpp"

#include <iostream>

Router::Router()
{
    get_root = new Node(false, NodeType::ROOT, "", {});
    post_root = new Node(false, NodeType::ROOT, "", {});
    put_root = new Node(false, NodeType::ROOT, "", {});
    delete_root = new Node(false, NodeType::ROOT, "", {});
    options_root = new Node(false, NodeType::ROOT, "", {});
    patch_root = new Node(false, NodeType::ROOT, "", {});
}

Router::~Router()
{
    if (get_root) delete get_root;
    if (post_root) delete post_root;
    if (put_root) delete put_root;
    if (delete_root) delete delete_root;
    if (options_root) delete options_root;
    if (patch_root) delete patch_root;
}

void Router::add_route(Method method, const std::string& path, const RouteHandler& handler)
{
    auto segments = get_segments(path);
    if (segments.empty()) { return; }

    Node* root = get_root_node(method);
    if (!root) { return; }

    Node* current = root;

    for (size_t i = 0; i < segments.size(); ++i)
    {
        const auto& segment = segments[i];
        NodeType type = get_node_type(segment);
        bool is_leaf = (i == segments.size() - 1);

        Node* next = nullptr;

        // Find existing child node based on type
        if (type == NodeType::STATIC)
        {
            auto it = current->children.find(segment);
            if (it != current->children.end()) { next = it->second; }
        }
        else if (type == NodeType::WILDCARD && current->wildcard_child) { next = current->wildcard_child; }
        else if (type == NodeType::NAMED_PARAMETER && current->param_child) { next = current->param_child; }
        else if (type == NodeType::REGEX_PARAMETER && current->regex_child) { next = current->regex_child; }

        if (!next)
        {
            next = new Node(is_leaf, type, segment, handler);
            current->add_child(next);
        }
        else if (is_leaf) { next->is_leaf = true; }

        current = next;
    }
}

Node* Router::find_route(Method method, const std::string& path, RouteParams& params)
{
    auto segments = get_segments(path);
    if (segments.empty()) { return nullptr; }

    Node* current = get_root_node(method);
    if (!current) { return nullptr; }

    for (size_t i = 0; i < segments.size(); ++i)
    {
        current = current->get_child(segments[i], params);
        if (!current) { return nullptr; }
    }

    return current->is_leaf ? current : nullptr;
}

std::vector<std::string> Router::get_segments(const std::string& path)
{
    std::vector<std::string> segments;
    std::string current;

    // Handle leading slash
    size_t start = (path.size() > 0 && path[0] == '/') ? 1 : 0;

    for (size_t i = start; i < path.size(); ++i)
    {
        if (path[i] == '/')
        {
            if (!current.empty())
            {
                segments.push_back(current);
                current.clear();
            }
        }
        else { current += path[i]; }
    }

    if (!current.empty()) { segments.push_back(current); }

    return segments;
}

NodeType Router::get_node_type(const std::string& segment)
{
    if (segment == "*") { return NodeType::WILDCARD; }
    else if (!segment.empty() && segment[0] == ':') { return NodeType::NAMED_PARAMETER; }
    else if (segment.length() > 2 && segment[0] == '{' && segment[segment.length() - 1] == '}' &&
             segment.find(':', 1) != std::string::npos)
    {
        return NodeType::REGEX_PARAMETER;
    }
    return NodeType::STATIC;
}

Node* Router::get_root_node(Method method)
{
    switch (method)
    {
        case Method::GET: return get_root;
        case Method::POST: return post_root;
        case Method::PUT: return put_root;
        case Method::DELETE: return delete_root;
        case Method::OPTIONS: return options_root;
        case Method::PATCH: return patch_root;
        default: return nullptr;
    }
}

void Router::print()
{
    std::cout << "GET Routes:\n";
    if (get_root != nullptr) { get_root->print(0); }

    std::cout << "\nPOST Routes:\n";
    if (post_root != nullptr) { post_root->print(0); }

    std::cout << "\nPUT Routes:\n";
    if (put_root != nullptr) { put_root->print(0); }

    std::cout << "\nDELETE Routes:\n";
    if (delete_root != nullptr) { delete_root->print(0); }

    std::cout << "\nOPTIONS Routes:\n";
    if (options_root != nullptr) { options_root->print(0); }

    std::cout << "\nPATCH Routes:\n";
    if (patch_root != nullptr) { patch_root->print(0); }
}

Node::Node(bool is_leaf, NodeType type, const std::string& path, const RouteHandler& handler)
    : is_leaf(is_leaf), type(type), path(path), handler(handler)
{
    if (type == NodeType::NAMED_PARAMETER)
    {
        // Extract parameter name (remove the ':')
        param_name = path.substr(1);
    }
    else if (type == NodeType::REGEX_PARAMETER)
    {
        // Extract parameter name and regex pattern from {name:pattern}
        size_t colon_pos = path.find(':', 1);
        if (colon_pos != std::string::npos && path.size() > 2 && path[0] == '{' && path[path.size() - 1] == '}')
        {
            param_name = path.substr(1, colon_pos - 1);
            std::string regex_str = path.substr(colon_pos + 1, path.size() - colon_pos - 2);
            try
            {
                pattern = std::regex(regex_str);
            }
            catch (const std::regex_error&)
            {
                std::cerr << "Invalid regex pattern: " << regex_str << std::endl;
            }
        }
    }
}

void Node::add_child(Node* child)
{
    if (child->type == NodeType::WILDCARD) { wildcard_child = child; }
    else if (child->type == NodeType::NAMED_PARAMETER) { param_child = child; }
    else if (child->type == NodeType::REGEX_PARAMETER) { regex_child = child; }
    else { children[child->path] = child; }
}

Node* Node::get_child(const std::string& path, RouteParams& params)
{
    // First try exact match
    auto it = children.find(path);
    if (it != children.end()) { return it->second; }

    // Try named parameter
    if (param_child)
    {
        params[param_child->param_name] = path;
        return param_child;
    }

    // Try regex parameter
    if (regex_child)
    {
        if (std::regex_match(path, regex_child->pattern))
        {
            params[regex_child->param_name] = path;
            return regex_child;
        } else {
            // Note (sarbaz) this would be wrong i think
            return nullptr;
        }
    }

    // Try wildcard
    if (wildcard_child) { return wildcard_child; }

    return nullptr;
}

Node::~Node()
{
    for (auto& [_, child] : children) { delete child; }
    if (wildcard_child) delete wildcard_child;
    if (param_child) delete param_child;
    if (regex_child) delete regex_child;
}

void Node::print(int depth)
{
    if (type != NodeType::ROOT)
    {
        for (int i = 0; i < depth; i++) { std::cout << "  "; }

        std::string type_str;
        switch (type)
        {
            case NodeType::ROOT: type_str = "ROOT"; break;
            case NodeType::STATIC: type_str = "STATIC"; break;
            case NodeType::WILDCARD: type_str = "WILDCARD"; break;
            case NodeType::NAMED_PARAMETER: type_str = "PARAM"; break;
            case NodeType::REGEX_PARAMETER: type_str = "REGEX"; break;
        }

        std::cout << path << " [" << type_str << "]" << (is_leaf ? " (endpoint)" : "") << "\n";
    }

    for (auto& [key, child] : children) { child->print(depth + 1); }
    if (wildcard_child) wildcard_child->print(depth + 1);
    if (param_child) param_child->print(depth + 1);
    if (regex_child) regex_child->print(depth + 1);
}