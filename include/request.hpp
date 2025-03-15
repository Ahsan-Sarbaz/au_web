#pragma once

#include "path.hpp"
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>
#include <unordered_map>

typedef std::unordered_map<std::string_view, std::string_view> Headers;

enum class Method
{
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    DELETE,
    TRACE,
    CONNECT,
    UNKNOWN
};

class Request
{
    friend class Connection;
public:
    static Request from_content(std::vector<char>&& content, size_t header_size);
    
private:
    void parse();
    std::string create_response() const;
    void print() const;

private:
    std::vector<char> content;
    Headers headers;
    Path path;
    Method method = Method::UNKNOWN;
    size_t header_size = 0;
    std::span<char> body;
    bool is_complete = false;
};