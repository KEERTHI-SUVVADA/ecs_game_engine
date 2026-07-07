cmake_minimum_required(VERSION 3.16)
project(ECSDemo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_DIR "E:/SFML-2.6.1/lib/cmake/SFML")
find_package(SFML 2.5 COMPONENTS graphics REQUIRED)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.4
)
FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
    imgui_sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG        v2.6
)
set(IMGUI_DIR ${imgui_SOURCE_DIR} CACHE PATH "" FORCE)
set(IMGUI_SFML_FIND_SFML OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(imgui_sfml)

add_executable(ecs_demo
    src/main.cpp
)

target_include_directories(ecs_demo PRIVATE src)

target_link_libraries(ecs_demo PRIVATE
    sfml-graphics
    ImGui-SFML::ImGui-SFML
)

if(WIN32)
    add_custom_command(TARGET ecs_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ecs_demo>
            $<TARGET_FILE_DIR:ecs_demo>
        COMMAND_EXPAND_LISTS
    )
endif()