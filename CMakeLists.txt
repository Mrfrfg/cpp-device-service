cmake_minimum_required(VERSION 3.16)

project(device_service LANGUAGES CXX)



option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

if(ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "ThreadSanitizer requires GCC or Clang")
    endif()

    add_compile_options(
        -fsanitize=thread
        -fno-omit-frame-pointer
        -g
    )

    add_link_options(-fsanitize=thread)
endif()


option(
    ENABLE_ASAN_UBSAN
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer"
    OFF
)

if(ENABLE_TSAN AND ENABLE_ASAN_UBSAN)
    message(FATAL_ERROR
        "TSan cannot be combined with ASan/UBSan")
endif()

if(ENABLE_ASAN_UBSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR
            "Sanitizers require GCC or Clang")
    endif()

    add_compile_options(
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -g
    )

    add_link_options(
        -fsanitize=address,undefined
    )
endif()

find_package(Threads REQUIRED)

add_library(device_store device_store.cpp)

target_compile_features(device_store PUBLIC cxx_std_17)

target_compile_options(device_store PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_include_directories(device_store PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(device_store PUBLIC
    Threads::Threads
)

add_executable(device_store_tests
    test_device_store.cpp
)

target_link_libraries(device_store_tests PRIVATE
    device_store
)

target_compile_options(device_store_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()

add_test(
    NAME device_store_tests
    COMMAND device_store_tests
)

add_executable(device_server
    server.cpp
)

target_link_libraries(device_server PRIVATE
    device_store
)

target_compile_options(device_server PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(device_client
    client.cpp
)

target_compile_features(device_client PRIVATE
    cxx_std_17
)

target_compile_options(device_client PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_test(
    NAME device_server_integration
    COMMAND bash
        ${CMAKE_CURRENT_SOURCE_DIR}/test_integration.sh
        $<TARGET_FILE:device_server>
        $<TARGET_FILE:device_client>
        19090
)

set_tests_properties(
    device_server_integration
    PROPERTIES
        TIMEOUT 10
        RUN_SERIAL TRUE
)
