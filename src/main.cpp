#include "application.hpp"
#include "request.hpp"
#include "response.hpp"
#include <iostream>

int main()
{
    Application app = Application(8080);

    // Add routes
    app.GET("/users", [](Request& req)-> Response {
        std::cout << "GET /users" << std::endl;
        (void) req;
        return Response::ok("/users");
    });
    
    app.GET("/users/:id", [](Request& req) -> Response {
        std::cout << "GET /users/:id" << std::endl;
        (void) req;
        return Response::ok("/users/" + req.params()["id"]);
    });
    
    app.GET("/users/:id/profile", [](Request& req) -> Response {
        std::cout << "GET /users/:id/profile" << std::endl;
        (void) req;
        return Response::ok("/users/" + req.params()["id"] + "/profile");
    });

    app.GET("/products/{category:[a-z]+}", [](Request& req) -> Response {
        std::cout << "GET /products/{category:[a-z]+}" << std::endl;
        (void) req;
        return Response::ok("/products/" + req.params()["category"]);
    });

    app.GET("/files/*", [](Request& req) -> Response {
        std::cout << "GET /files/*" << std::endl;
        (void) req;
        return Response::ok("/files/" + req.params()["*"]);
    });
    
    app.run();

    return 0;
}