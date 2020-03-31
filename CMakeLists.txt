cmake_minimum_required(VERSION 2.8)

project(RedditClientSDL)
add_executable(${PROJECT_NAME} "main.cpp" "httpsc.h" "stb_image.h")
target_link_libraries(${PROJECT_NAME} SDL2 SDL2_ttf boost_system pthread crypto ssl)
target_include_directories(${PROJECT_NAME} PUBLIC rapidjson/include)
