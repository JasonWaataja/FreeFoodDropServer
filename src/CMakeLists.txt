set (SOURCES main.c)
add_executable (ffd ${SOURCES})

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
message (${CMAKE_MODULE_PATH})

find_package (Threads REQUIRED)
find_package (MySQL REQUIRED)

include_directories (${MYSQL_INCLUDE_DIR})
target_link_libraries (ffd ${CMAKE_THREAD_LIBS_INIT} ${MYSQL_LIBRARY})
