cmake_minimum_required(VERSION 3.28)

project(web)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

set (SOURCE 
    src/main.cpp
    src/server.cpp
    src/connection.cpp
    src/request.cpp
    src/path.cpp
)

set (HEADERS 
    include/request.hpp
    include/connection.hpp
    include/path.hpp
    include/server.hpp
)

include_directories(include)

add_executable(${PROJECT_NAME} ${SOURCE} ${HEADERS})
