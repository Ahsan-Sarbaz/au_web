#pragma once
#include "request.hpp"
#include "router.hpp"
#include "server.hpp"

class Application
{
  public:
    Application(int port) : server(port, router)
    {
    }
    
    void run()
    {
        server.run();
    }

    inline void GET(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::GET, route, handler);
    }

    inline void POST(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::POST, route, handler);
    }
    
    inline void PUT(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::PUT, route, handler);
    }
    
    inline void DELETE(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::DELETE, route, handler);
    }
    
    inline void PATCH(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::PATCH, route, handler);
    }
    
    inline void OPTIONS(const std::string& route, const RouteHandler& handler)
    {
        router.add_route(Method::OPTIONS, route, handler);
    }

  private:
    Server server;
    Router router;
};