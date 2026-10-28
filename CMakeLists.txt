cmake_minimum_required(VERSION 3.5)
project(poly3d_packing CXX)

set(CMAKE_CXX_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_library(plp
  src/ConvexPolytope.cpp
  src/Json.cpp
  src/Mesh.cpp
  src/Instance.cpp
  src/Solution.cpp
  src/SmallLP.cpp
  src/Placer.cpp
  src/Greedy.cpp
)
target_include_directories(plp PUBLIC src third_party)

# Qhull (optional): robust convex hulls of large point sets (parts of curved meshes)
find_path(QHULL_INCLUDE_DIR libqhull_r/libqhull_r.h)
find_library(QHULL_LIBRARY qhull_r)
if(QHULL_INCLUDE_DIR AND QHULL_LIBRARY)
  target_compile_definitions(plp PUBLIC PLP_WITH_QHULL)
  target_include_directories(plp PUBLIC ${QHULL_INCLUDE_DIR})
  target_link_libraries(plp PUBLIC ${QHULL_LIBRARY})
  message(STATUS "Qhull: ${QHULL_LIBRARY}")
endif()

add_executable(poly3d src/main.cpp)
target_link_libraries(poly3d plp)

add_executable(poly3d_check src/check.cpp)
target_link_libraries(poly3d_check plp)

add_executable(poly3d_test src/test.cpp)
target_link_libraries(poly3d_test plp)

enable_testing()
set(B ${CMAKE_CURRENT_SOURCE_DIR}/benchs)
add_test(NAME unit COMMAND poly3d_test ${B})
add_test(NAME check_stoyan2005 COMMAND poly3d_check -q "${B}/Stoyan et al. 2005/Example1.json"
  -s "${B}/Stoyan et al. 2005/Solution1.json")
add_test(NAME solve_stoyan2005 COMMAND poly3d -t 2 "${B}/Stoyan et al. 2005/Example1.json")
