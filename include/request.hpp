#pragma once

#include "path.hpp"
#include <string_view>
#include <vector>

struct Header
{
    std::string_view name;
    std::string_view value;
};

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
    static Request from_content(std::vector<char>&& content);

private:
    bool check_complete();
    void parse();
    std::string create_response() const;
    void print() const;

private:
    std::vector<char> content;
    std::vector<Header> headers;
    std::vector<std::string_view> lines;
    Path path;
    Method method = Method::UNKNOWN;
    bool is_complete = false;
};