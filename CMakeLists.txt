cmake_minimum_required(VERSION 3.4)

project(pydical)

find_package(pybind11 REQUIRED)

pybind11_add_module(pydical src/main.cpp)
