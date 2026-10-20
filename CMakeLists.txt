# Standalone build of the Riskbound simulation core and its unit tests.
# The same sources compile inside Unreal as the RiskboundSim module (Source/RiskboundSim).
cmake_minimum_required(VERSION 3.20)
project(RiskboundCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB RB_SIM_SOURCES CONFIGURE_DEPENDS Source/RiskboundSim/Private/Rb*.cpp)
add_library(RiskboundSimCore STATIC ${RB_SIM_SOURCES})
target_include_directories(RiskboundSimCore PUBLIC Source/RiskboundSim/Public)
# UE builds without exceptions or RTTI; keep the core honest about that.
if(MSVC)
  target_compile_options(RiskboundSimCore PRIVATE /W4 /GR-)
else()
  target_compile_options(RiskboundSimCore PRIVATE -Wall -Wextra -Wpedantic -Wshadow -fno-exceptions -fno-rtti)
endif()

enable_testing()
file(GLOB RB_TEST_SOURCES CONFIGURE_DEPENDS Tests/Core/*.cpp)
add_executable(RiskboundCoreTests ${RB_TEST_SOURCES})
target_link_libraries(RiskboundCoreTests PRIVATE RiskboundSimCore)
add_test(NAME RiskboundCoreTests COMMAND RiskboundCoreTests)
