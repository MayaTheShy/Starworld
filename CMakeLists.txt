cmake_minimum_required(VERSION 3.15)
project(starworld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_OVERTE_SDK "Link against Overte SDK if available" OFF)
option(USE_STARDUST_SDK "Link against StardustXR SDK if available" OFF)
option(USE_OVERTE_NETWORKING "Link against Overte networking library" OFF)  # Disabled, using custom impl

find_package(glm REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Find Qt5 for Overte networking library
if(USE_OVERTE_NETWORKING)
    find_package(Qt5 COMPONENTS Core Network REQUIRED)
    set(CMAKE_AUTOMOC ON)
endif()

add_executable(starworld
    src/main.cpp
    src/StardustBridge.cpp
    src/OverteClient.cpp
    src/SceneSync.cpp
    src/InputHandler.cpp
    src/NLPacketCodec.cpp
    src/DomainDiscovery.cpp
    src/ModelCache.cpp
 )

add_executable(starworld-tests
    tests/TestHarness.cpp
    src/NLPacketCodec.cpp
    src/DomainDiscovery.cpp
)

find_package(CURL REQUIRED)
target_link_libraries(starworld PRIVATE glm::glm ZLIB::ZLIB CURL::libcurl OpenSSL::Crypto)
target_link_libraries(starworld-tests PRIVATE glm::glm ZLIB::ZLIB OpenSSL::Crypto CURL::libcurl)

# Link Overte networking library
if(USE_OVERTE_NETWORKING)
    # Add Overte include directories
    target_include_directories(stardust-overte-client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/overte-src/libraries/networking/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/overte-src/libraries/shared/src
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/overte-src/libraries/platform/src
    )
    
    # Link against precompiled Overte networking library
    target_link_libraries(stardust-overte-client PRIVATE
        /opt/overte/lib/libnetworking.so
        Qt5::Core
        Qt5::Network
    )
    
    target_compile_definitions(stardust-overte-client PRIVATE HAVE_OVERTE_NETWORKING=1)
endif()

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
