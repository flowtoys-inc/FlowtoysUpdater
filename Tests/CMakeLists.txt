include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG 317ac1ed4c0bb6e6b91eafc817e05c488feffcb3 # v3.16.0
)
FetchContent_MakeAvailable(Catch2)

juce_add_console_app(FlowtoysUpdaterTests PRODUCT_NAME "FlowtoysUpdaterTests")

target_sources(FlowtoysUpdaterTests PRIVATE
    VersionUtilsTests.cpp
    FirmwareImageTests.cpp
    PropConstantsTests.cpp)

target_include_directories(FlowtoysUpdaterTests PRIVATE ${CMAKE_SOURCE_DIR}/Source)

target_link_libraries(FlowtoysUpdaterTests PRIVATE
    Catch2::Catch2WithMain
    juce::juce_core
    juce::juce_recommended_config_flags)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(FlowtoysUpdaterTests)
