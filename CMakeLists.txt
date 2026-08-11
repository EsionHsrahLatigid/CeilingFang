cmake_minimum_required(VERSION 3.31)

if(APPLE)
    project(CeilingFang VERSION 0.1.0 LANGUAGES C CXX OBJC OBJCXX)
else()
    project(CeilingFang VERSION 0.1.0 LANGUAGES C CXX)
endif()

option(CEILINGFANG_BUILD_PLUGIN "Build the YUP Standalone/VST3/AU targets" ON)
option(CEILINGFANG_BUILD_TESTS "Build deterministic DSP regression tests" ON)
option(CEILINGFANG_ADHOC_SIGN "Ad-hoc sign local macOS plugin and app bundles after building" ON)
set(CEILINGFANG_CREATE_AU OFF)
if(APPLE)
    set(CEILINGFANG_CREATE_AU ON)
endif()
set(CEILINGFANG_YUP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../yup" CACHE PATH
    "Path to a local YUP checkout; fetched from upstream when unavailable")

add_library(ceilingfang_engine STATIC
    source/CeilingFangEngine.cpp)
target_include_directories(ceilingfang_engine PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include")
target_compile_features(ceilingfang_engine PUBLIC cxx_std_20)
set_target_properties(ceilingfang_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ceilingfang_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(CEILINGFANG_BUILD_TESTS)
    include(CTest)

    function(enable_test_assertions target_name)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /UNDEBUG)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target_name} PRIVATE -UNDEBUG)
        endif()
    endfunction()

    add_executable(ceilingfang_engine_tests
        tests/CeilingFangEngineTests.cpp)
    target_link_libraries(ceilingfang_engine_tests PRIVATE ceilingfang_engine)
    target_compile_features(ceilingfang_engine_tests PRIVATE cxx_std_20)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(ceilingfang_engine_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    enable_test_assertions(ceilingfang_engine_tests)
    add_test(NAME ceilingfang_engine_tests COMMAND ceilingfang_engine_tests)
endif()

if(CEILINGFANG_BUILD_PLUGIN)
    set(YUP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_PLUGINS OFF CACHE BOOL "" FORCE)
    set(YUP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_PLUGINVAL OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_VST3_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_CLAP_VALIDATOR OFF CACHE BOOL "" FORCE)
    set(YUP_ENABLE_AUVAL_VALIDATOR OFF CACHE BOOL "" FORCE)

    if(EXISTS "${CEILINGFANG_YUP_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory("${CEILINGFANG_YUP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/yup")
    else()
        include(FetchContent)
        FetchContent_Declare(yup
            GIT_REPOSITORY https://github.com/kunitoki/yup.git
            GIT_TAG 9a1c9bc699b6a714f6f52486462d98a140c8bf95)
        FetchContent_MakeAvailable(yup)
    endif()

    # YUP's iconutil-based helper rejects its generated iconset on current
    # macOS. Keep the compatibility fix project-local until upstream changes.
    if(APPLE)
        include(cmake/YupMacOSIconWorkaround.cmake)
    endif()

    yup_audio_plugin(
        TARGET_NAME ceilingfang
        TARGET_VERSION ${PROJECT_VERSION}
        TARGET_IDE_GROUP "CeilingFang"
        TARGET_APP_ID "jp.ehl.ceilingfang"
        TARGET_APP_NAMESPACE "jp.ehl"
        TARGET_CXX_STANDARD 20
        PLUGIN_ID "jp.ehl.ceilingfang"
        PLUGIN_NAME "CeilingFang"
        PLUGIN_VENDOR "ehl_"
        PLUGIN_EMAIL ""
        PLUGIN_VERSION "${PROJECT_VERSION}"
        PLUGIN_DESCRIPTION "Lookahead inter-sample peak limiter with adaptive release and zero-cross bias"
        PLUGIN_URL "https://github.com/EsionHsrahLatigid/CeilingFang"
        PLUGIN_IS_SYNTH OFF
        PLUGIN_IS_MONO OFF
        PLUGIN_CREATE_CLAP OFF
        PLUGIN_CREATE_VST3 ON
        PLUGIN_CREATE_AU ${CEILINGFANG_CREATE_AU}
        PLUGIN_AU_SUBTYPE "ClFn"
        PLUGIN_AU_MANUFACTURER "EHL1"
        PLUGIN_AU_SANDBOX_SAFE ON
        PLUGIN_CREATE_AUv3 OFF
        PLUGIN_CREATE_LV2 OFF
        PLUGIN_CREATE_AAX OFF
        PLUGIN_CREATE_STANDALONE ON
        PLUGIN_COPY_AFTER_BUILD OFF
        MODULES
            yup::yup_gui
            yup::yup_audio_processors)

    target_sources(ceilingfang_shared INTERFACE
        source/ParameterGridEditor.cpp
        source/ParameterGridEditor.h
        source/ProductState.h
        source/CeilingFangPlugin.cpp
        source/CeilingFangPlugin.h)
    target_link_libraries(ceilingfang_shared INTERFACE ceilingfang_engine)
    target_include_directories(ceilingfang_shared INTERFACE
        "${CMAKE_CURRENT_LIST_DIR}/include"
        "${CMAKE_CURRENT_LIST_DIR}/source")

    if(CEILINGFANG_BUILD_TESTS)
        add_executable(ceilingfang_plugin_bridge_tests
            tests/CeilingFangPluginBridgeTests.cpp
            source/CeilingFangPlugin.cpp)
        target_link_libraries(ceilingfang_plugin_bridge_tests PRIVATE ceilingfang_engine yup::yup_audio_processors)
        target_include_directories(ceilingfang_plugin_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(ceilingfang_plugin_bridge_tests PRIVATE CEILINGFANG_HEADLESS_TEST=1)
        target_compile_features(ceilingfang_plugin_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(ceilingfang_plugin_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(ceilingfang_plugin_bridge_tests)
        add_test(NAME ceilingfang_plugin_bridge_tests COMMAND ceilingfang_plugin_bridge_tests)

        add_executable(ceilingfang_standalone_bridge_tests
            tests/CeilingFangStandaloneBridgeTests.cpp
            source/CeilingFangPlugin.cpp)
        target_link_libraries(ceilingfang_standalone_bridge_tests PRIVATE ceilingfang_engine yup::yup_audio_processors)
        target_include_directories(ceilingfang_standalone_bridge_tests PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/include"
            "${CMAKE_CURRENT_LIST_DIR}/source")
        target_compile_definitions(ceilingfang_standalone_bridge_tests PRIVATE
            CEILINGFANG_HEADLESS_TEST=1
            YUP_AUDIO_PLUGIN_ENABLE_STANDALONE=1)
        target_compile_features(ceilingfang_standalone_bridge_tests PRIVATE cxx_std_20)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(ceilingfang_standalone_bridge_tests PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        enable_test_assertions(ceilingfang_standalone_bridge_tests)
        add_test(NAME ceilingfang_standalone_bridge_tests COMMAND ceilingfang_standalone_bridge_tests)
    endif()

    set(ceilingfang_release_bundle_targets
        ceilingfang_standalone_plugin
        ceilingfang_vst3_plugin)
    if(APPLE)
        list(APPEND ceilingfang_release_bundle_targets ceilingfang_au_plugin)
    endif()

    add_custom_target(ceilingfang_release_bundles
        DEPENDS ${ceilingfang_release_bundle_targets})

    include(cmake/EhlYupArtifactLayout.cmake)
    ehl_yup_add_artifact_target(
        TARGET ehl_stage_products
        PRODUCT CeilingFang
        SLUG ceilingfang
        STANDALONE_TARGET ceilingfang_standalone_plugin
        VST3_TARGET ceilingfang_vst3_plugin
        AU_TARGET ceilingfang_au_plugin)

    if(APPLE)
        set_target_properties(ceilingfang_standalone_plugin PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "jp.ehl.ceilingfang"
            MACOSX_BUNDLE_BUNDLE_NAME "CeilingFang"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "jp.ehl.ceilingfang")
    endif()

    if(APPLE AND CEILINGFANG_ADHOC_SIGN)
        find_program(CEILINGFANG_CODESIGN_EXECUTABLE codesign REQUIRED)
        add_custom_command(TARGET ceilingfang_vst3_plugin POST_BUILD
            COMMAND "${CEILINGFANG_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:ceilingfang_vst3_plugin>"
            COMMENT "Ad-hoc signing CeilingFang VST3 for local loading"
            VERBATIM)
        add_custom_command(TARGET ceilingfang_standalone_plugin POST_BUILD
            COMMAND "${CEILINGFANG_CODESIGN_EXECUTABLE}" --force --deep --sign -
                    "$<TARGET_BUNDLE_DIR:ceilingfang_standalone_plugin>"
            COMMENT "Ad-hoc signing CeilingFang standalone app for local launch"
            VERBATIM)
    endif()
endif()
