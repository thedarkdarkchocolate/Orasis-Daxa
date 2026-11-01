include(FetchContent)

# GLFW - Daxa only fetches GLFW for its own tests, so the app has to provide it.
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glfw)

# Vulkan - Daxa needs the Vulkan SDK's headers (Vulkan 1.4 + glslang/SPIRV). Without this, a vcpkg
# toolchain makes find_package(Vulkan) pick up vcpkg's older vulkan-headers instead, so resolve the
# variables FindVulkan uses from the SDK up front.
if(DEFINED ENV{VULKAN_SDK})
    find_path(Vulkan_INCLUDE_DIR NAMES vulkan/vulkan.h HINTS "$ENV{VULKAN_SDK}/include" NO_DEFAULT_PATH)
    find_library(Vulkan_LIBRARY NAMES vulkan-1 vulkan HINTS "$ENV{VULKAN_SDK}/lib" NO_DEFAULT_PATH)
endif()

# Daxa - the DAXA_ENABLE_UTILS_* options must be set before Daxa's CMakeLists.txt runs.
set(DAXA_ENABLE_UTILS_PIPELINE_MANAGER_GLSLANG ON)
set(DAXA_ENABLE_UTILS_TASK_GRAPH ON)
set(DAXA_ENABLE_TESTS OFF)
set(DAXA_ENABLE_TOOLS OFF)
FetchContent_Declare(
    daxa
    GIT_REPOSITORY https://github.com/Ipotrick/Daxa
    # Pinned: the next commit (52ba9ea, cooperative vector) needs newer Vulkan headers than SDK 1.4.304.
    GIT_TAG        3a561af0119873a53ed6deed988c06ec0aeb1e77
    GIT_SHALLOW    FALSE # a shallow clone can't check out a commit hash
)
FetchContent_MakeAvailable(daxa)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# cgltf - single-header glTF loader. It has no CMakeLists.txt, so the target is declared here.
# The implementation is compiled once, in src/cgltf.cpp.
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf
    GIT_TAG        v1.15
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(cgltf)
add_library(cgltf INTERFACE)
target_include_directories(cgltf SYSTEM INTERFACE ${cgltf_SOURCE_DIR})

# stb_image - single-header image decoder (PNG, JPEG, ...). Only the one header is needed, so it is
# downloaded on its own instead of cloning the whole stb repository.
# The implementation is compiled once, in src/stb_image.cpp.
set(STB_IMAGE_DIR ${CMAKE_BINARY_DIR}/_deps/stb_image)
if(NOT EXISTS ${STB_IMAGE_DIR}/stb_image.h)
    file(DOWNLOAD
        https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
        ${STB_IMAGE_DIR}/stb_image.h
        STATUS STB_IMAGE_DOWNLOAD_STATUS
    )
    list(GET STB_IMAGE_DOWNLOAD_STATUS 0 STB_IMAGE_DOWNLOAD_CODE)
    if(NOT STB_IMAGE_DOWNLOAD_CODE EQUAL 0)
        file(REMOVE ${STB_IMAGE_DIR}/stb_image.h)
        message(FATAL_ERROR "Failed to download stb_image.h: ${STB_IMAGE_DOWNLOAD_STATUS}")
    endif()
endif()
add_library(stb_image INTERFACE)
target_include_directories(stb_image SYSTEM INTERFACE ${STB_IMAGE_DIR})