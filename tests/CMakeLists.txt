find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Test)

add_executable(test_gpuinfo
  test_gpuinfo.cpp
  ${CMAKE_SOURCE_DIR}/src/gpuinfo.cpp
)
target_include_directories(test_gpuinfo PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_gpuinfo PRIVATE Qt6::Core Qt6::Test)
if(WIN32)
  target_link_libraries(test_gpuinfo PRIVATE dxgi dxguid)
endif()
add_test(NAME gpuinfo COMMAND test_gpuinfo)

add_executable(test_mix_transition_order
  test_mix_transition_order.cpp
  ${CMAKE_SOURCE_DIR}/src/models/mixtransitionorder.cpp
)
target_include_directories(test_mix_transition_order PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mix_transition_order PRIVATE Qt6::Core Qt6::Test PkgConfig::mlt++)
add_test(NAME mix_transition_order COMMAND test_mix_transition_order)

add_executable(test_simple_edit
  test_simple_edit.cpp
  ${CMAKE_SOURCE_DIR}/src/simple/simpleedit.cpp
)
target_include_directories(test_simple_edit PRIVATE ${CMAKE_SOURCE_DIR}/src/simple)
target_link_libraries(test_simple_edit PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME simple_edit COMMAND test_simple_edit)

add_executable(test_simple_pipeline
  test_simple_pipeline.cpp
  ${CMAKE_SOURCE_DIR}/src/simple/simpleedit.cpp
  ${CMAKE_SOURCE_DIR}/src/simple/simplemedia.cpp
  ${CMAKE_SOURCE_DIR}/src/simple/collage.cpp
)
target_include_directories(test_simple_pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src/simple ${CMAKE_SOURCE_DIR}/CuteLogger/include)
target_link_libraries(test_simple_pipeline PRIVATE Qt6::Core Qt6::Gui Qt6::Test CuteLogger PkgConfig::mlt++)
add_test(NAME simple_pipeline COMMAND test_simple_pipeline)

add_executable(test_collage
  test_collage.cpp
  ${CMAKE_SOURCE_DIR}/src/simple/collage.cpp
)
target_include_directories(test_collage PRIVATE ${CMAKE_SOURCE_DIR}/src/simple)
target_link_libraries(test_collage PRIVATE Qt6::Core Qt6::Test)
add_test(NAME simple_collage COMMAND test_collage)
