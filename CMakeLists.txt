cmake_minimum_required(VERSION 3.15)
project(stardust-overte-client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_OVERTE_SDK "Link against Overte SDK if available" OFF)
option(USE_STARDUST_SDK "Link against StardustXR SDK if available" OFF)

find_package(glm REQUIRED)

add_executable(stardust-overte-client
    src/main.cpp
    src/StardustBridge.cpp
    src/OverteClient.cpp
    src/SceneSync.cpp
    src/InputHandler.cpp
)

target_link_libraries(stardust-overte-client PRIVATE glm::glm)

if(USE_OVERTE_SDK)
    find_package(Overte QUIET)
    if(Overte_FOUND)
        target_link_libraries(stardust-overte-client PRIVATE Overte::Networking)
        target_compile_definitions(stardust-overte-client PRIVATE HAVE_OVERTE_SDK=1)
    endif()
endif()

if(USE_STARDUST_SDK)
    find_package(StardustXR QUIET)
    if(StardustXR_FOUND)
        target_link_libraries(stardust-overte-client PRIVATE StardustXR::Client)
        target_compile_definitions(stardust-overte-client PRIVATE HAVE_STARDUST_SDK=1)
    endif()
endif()

# Optional: try to locate a prebuilt Rust bridge shared library at runtime using RPATH hints
if(NOT DEFINED STARWORLD_BRIDGE_PATH)
    set(STARWORLD_BRIDGE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bridge/target/debug" CACHE PATH "Path to Rust bridge .so/.dylib directory")
endif()
if(EXISTS "${STARWORLD_BRIDGE_PATH}")
    # Add to rpath so dlopen without full path can find it
    set_target_properties(stardust-overte-client PROPERTIES
        BUILD_RPATH "${STARWORLD_BRIDGE_PATH}"
        INSTALL_RPATH "${STARWORLD_BRIDGE_PATH}")
endif()
