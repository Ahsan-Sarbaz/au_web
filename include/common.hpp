#pragma once

#include <functional>
#include <string>
#include <unordered_map>

enum class Method
{
    OPTIONS,
    GET,
    HEAD,
    POST,
    PUT,
    PATCH,
    DELETE,
    TRACE,
    CONNECT,
    UNKNOWN
};

class Request;
class Response;

using RouteParams = std::unordered_map<std::string, std::string>;
using RouteHandler = std::function<Response(Request&)>;
