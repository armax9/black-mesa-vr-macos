cmake_minimum_required(VERSION 3.20)
project(BMVRCompatibilityChecks LANGUAGES CXX)
add_executable(BMVRCompatibilityChecks compatibility.cpp)
target_compile_features(BMVRCompatibilityChecks PRIVATE cxx_std_17)
target_include_directories(BMVRCompatibilityChecks PRIVATE ../..)

target_compile_definitions(BMVRCompatibilityChecks PRIVATE NOMINMAX)
