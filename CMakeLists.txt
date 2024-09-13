cmake_minimum_required(VERSION 3.22.1)

project(rvm_parser)

set(CMAKE_CXX_STANDARD 17)

include_directories(libs)
include_directories(libs/libtess2/Include)
file(GLOB LIBTESS2_SRC "libs/libtess2/Source/*.c")
add_library(libtess2 STATIC ${LIBTESS2_SRC})
include_directories(libs/rapidjson/include)
add_subdirectory(libs/argumentum)
add_subdirectory(libs/tinygltf)
file(GLOB MESHOPTIMIZER_SRC "libs/meshoptimizer-0.21/src/*.cpp")
add_library(MESHOPT STATIC ${MESHOPTIMIZER_SRC})


# TODO
# ADD LINT RULES/COMPILER WARNINGS


add_executable(
    rvm_parser
    ./src/md5.cpp
    ./src/Arena.cpp
    ./src/main.cpp
    ./src/RvmParser.cpp
    ./src/RvmParser_generate_glb.cpp
    ./src/RvmParser_parse_and_read.cpp
    ./src/RvmParser_generate_status_file.cpp
    ./src/LinAlgOps.cpp
    ./src/Tessellator.cpp
    ./src/TriangulationFactory.cpp
)

target_link_libraries(rvm_parser 
    libtess2 
    MESHOPT
    Argumentum::headers
    -static
    )
