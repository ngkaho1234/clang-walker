cmake_minimum_required(VERSION 2.6)

project (clang-walker)
option(USE_CLANG "build application with clang" ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(Boost REQUIRED)
find_package(LibClang REQUIRED)
find_package(BerkeleyDB REQUIRED)

set(COMMON clang-walker-db.cxx)

include_directories(
    SYSTEM
    ${Boost_INCLUDE_DIRS}
    ${LibClang_INCLUDE_DIRS}
    ${DB_INCLUDE_DIRS}
)

add_executable(clang-walker ${COMMON} clang-walker.cxx)
target_link_libraries(clang-walker
    ${LibClang_LIBRARIES}
    ${DB_LIBRARIES}
)

add_executable(clang-walker-query ${COMMON} clang-walker-query.cxx)
target_link_libraries(clang-walker-query
    ${LibClang_LIBRARIES}
    ${DB_LIBRARIES}
)

add_executable(clang-walker-del ${COMMON} clang-walker-del.cxx)
target_link_libraries(clang-walker-del
    ${LibClang_LIBRARIES}
    ${DB_LIBRARIES}
)
