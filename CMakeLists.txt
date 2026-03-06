cmake_minimum_required(VERSION 3.10)
project(c-api-demo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT QISKIT_ROOT)
  set(QISKIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/qiskit")
endif()
if(NOT QISKIT_CPP_ROOT)
  set(QISKIT_CPP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/qiskit-cpp")
endif()
if(NOT QRMI_ROOT)
  set(QRMI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/qrmi")
endif()
if(NOT SBD_ROOT)
  set(SBD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/deps/sbd")
endif()

find_library(ACCELERATE_LIBRARY Accelerate)
if (ACCELERATE_LIBRARY)
  message(STATUS "Using Accelerate Framework")
else()
  set(BLA_VENDOR OpenBLAS)
  find_package(BLAS REQUIRED)
  include_directories(${BLAS_INCLUDE_DIRS})
  link_libraries(${BLAS_LIBRARIES})
endif()

# Eigen library
find_package(Eigen3 REQUIRED)

# MPI library
find_package(MPI REQUIRED)

# OpenMP library
find_package(OpenMP REQUIRED)

# qiskit_c_api library
add_library(qiskit_c_api STATIC IMPORTED)

# nlohmann_json
include(FetchContent)
FetchContent_Declare(
    json
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

# qiskit library
add_library(qiskit_cpp INTERFACE)
if(SQC_ROOT)
  target_include_directories(qiskit_cpp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/qiskit-cpp/src
    ${SQC_ROOT}/include
    # nlohmann_json::nlohmann_json
  )
  target_link_directories(qiskit_cpp INTERFACE
    ${SQC_ROOT}/lib
    ${SQC_ROOT}/lib64
  )
  target_link_libraries(qiskit_cpp INTERFACE
    "-Wl,-rpath ${SQC_ROOT}/lib -Wl,-rpath ${SQC_ROOT}/lib64" # dll
    $ENV{SQC_LIBS}
  )
else()
  target_include_directories(qiskit_cpp INTERFACE
      ${CMAKE_CURRENT_SOURCE_DIR}/deps/qiskit-cpp/src
      ${QRMI_ROOT}
      # nlohmann_json::nlohmann_json
  )
endif()

# ffsim library
# file(GLOB_RECURSE FFSIM_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/ffsim/src/*.cpp)
# add_library(ffsim STATIC ${FFSIM_SOURCES})
add_library(ffsim INTERFACE)
target_include_directories(ffsim INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/ffsim/include
        ${QISKIT_ROOT}/dist/c/include
)
target_link_libraries(ffsim INTERFACE Eigen3::Eigen)


# sqd addon
add_library(sqd-addon INTERFACE)
target_include_directories(sqd-addon INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/qiskit-addon-sqd-hpc/include
)

# boost::dynamic_bitset
add_library(boost_dynamic_bitset INTERFACE)
target_include_directories(boost_dynamic_bitset INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/assert/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/config/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/container_hash/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/describe/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/dynamic_bitset/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/integer/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/move/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/mp11/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/static_assert/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/throw_exception/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/boost/type_traits/include
)

# sbd library
add_library(sbd INTERFACE)
target_include_directories(sbd INTERFACE
    ${SBD_ROOT}/include
    ${MPI_C_INCLUDE_DIRS}
)
target_link_libraries(sbd INTERFACE MPI::MPI_C)

add_executable(c-api-demo src/main.cpp)


if (MSVC)
    target_link_directories(c-api-demo
        PUBLIC
            ${QISKIT_ROOT}/target/release
            ${QRMI_ROOT}/target/release
    )
    target_link_libraries(c-api-demo
        ffsim
        sqd-addon
        boost_dynamic_bitset
        sbd
        OpenMP::OpenMP_CXX
        MPI::MPI_CXX
        nlohmann_json::nlohmann_json
        qiskit_cext.dll.lib
        qrmi.dll.lib
        -lopenblas
    )
elseif(APPLE)
    target_link_directories(c-api-demo
      PUBLIC
        ${QISKIT_ROOT}/dist/c/lib
        ${QRMI_ROOT}/target/release
    )
    target_link_libraries(c-api-demo
        qiskit
        qrmi
        ffsim
        sqd-addon
        boost_dynamic_bitset
        sbd
        OpenMP::OpenMP_CXX
        MPI::MPI_CXX
        nlohmann_json::nlohmann_json
        ${ACCELERATE_LIBRARY}
    )
else()
    target_link_directories(c-api-demo PUBLIC ${QISKIT_ROOT}/dist/c/lib)
    target_link_libraries(c-api-demo
      PUBLIC
      #" -Wl,-rpath ${QISKIT_ROOT}/dist/c/lib -Wl,-rpath ${QRMI_ROOT}/target/release"
        qiskit
        ffsim
        sqd-addon
        boost_dynamic_bitset
        sbd
        OpenMP::OpenMP_CXX
        MPI::MPI_CXX
        nlohmann_json::nlohmann_json
        -lopenblas
    )
    if(SQC_ROOT)
      target_link_directories(qiskit_cpp INTERFACE
        ${SQC_ROOT}/lib
        ${SQC_ROOT}/lib64
      )
      target_link_libraries(qiskit_cpp INTERFACE
        "-Wl,-rpath ${SQC_ROOT}/lib -Wl,-rpath ${SQC_ROOT}/lib64" # dll
        $ENV{SQC_LIBS}
      )
    else()
      target_link_directories(c-api-demo PUBLIC ${QRMI_ROOT}/target/release)
      target_link_libraries(c-api-demo PUBLIC qrmi)
    endif()
endif()

target_include_directories(c-api-demo
    PRIVATE
        ${QISKIT_ROOT}/dist/c/include
        #${QRMI_ROOT}
        ${QISKIT_CPP_ROOT}/src
)

#if(SQC_ROOT)
#else()
#endif()


target_compile_options(c-api-demo PRIVATE "-DQRMI_ROOT=${QRMI_ROOT}")
