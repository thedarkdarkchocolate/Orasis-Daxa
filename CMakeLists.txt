cmake_minimum_required(VERSION 3.21)

project(learndaxa VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Fetches GLFW and Daxa via FetchContent (must come after project()).
include("${CMAKE_CURRENT_LIST_DIR}/cmake/deps.cmake")

file(GLOB_RECURSE SRCS ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_executable(${PROJECT_NAME} ${SRCS})

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Absolute path to the shaders, so they are found regardless of the working directory the app is run from.
target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_DIR="${PROJECT_SOURCE_DIR}/src/shader")

# Absolute path to the assets (models, textures), for the same reason.
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSET_DIR="${PROJECT_SOURCE_DIR}/assets")

target_link_libraries(${PROJECT_NAME} PRIVATE glfw daxa::daxa glm::glm cgltf stb_image)
