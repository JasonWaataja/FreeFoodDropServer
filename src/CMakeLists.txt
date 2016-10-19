set (SOURCES main.c)
add_executable (ffd ${SOURCES})

find_package (Threads)
target_link_libraries (ffd ${CMAKE_THREAD_LIBS_INIT})
