cmake_minimum_required(VERSION 3.16)
project(Josts VERSION 1.1.1 LANGUAGES CXX RC)
option(JOSTS_REQUIRE_X86 "Exigir el ejecutable portable de 32 bits" OFF)
if(JOSTS_REQUIRE_X86 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "La entrega portable debe compilarse como x86 (32 bits).")
endif()
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MINGW)
  set_property(SOURCE res/josts.rc APPEND PROPERTY COMPILE_OPTIONS --codepage=65001)
endif()
add_compile_definitions(UNICODE _UNICODE NOMINMAX WINVER=0x0601 _WIN32_WINNT=0x0601)
foreach(font Regular Medium Bold Italic)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/DM_Sans/DMSans-${font}.ttf")
    message(FATAL_ERROR "Falta DM Sans ${font}. Ejecute preparar_fuentes.ps1 antes de compilar.")
  endif()
endforeach()
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/res/MaterialIcons-Regular.ttf")
  message(FATAL_ERROR "Falta res/MaterialIcons-Regular.ttf de Google Material Icons.")
endif()
set_property(SOURCE res/josts.rc APPEND PROPERTY OBJECT_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/res/josts.manifest"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/ui_test.manifest"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.ico"
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE_DM_Sans.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE_Material_Icons.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE_LLVM_MinGW.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE_EN.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/hosts.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/MaterialIcons-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/DM_Sans/DMSans-Regular.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/DM_Sans/DMSans-Medium.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/DM_Sans/DMSans-Bold.ttf"
  "${CMAKE_CURRENT_SOURCE_DIR}/res/DM_Sans/DMSans-Italic.ttf")
add_executable(Josts WIN32
  src/main.cpp src/ui.cpp src/privileged_ops.cpp src/edge_settings.cpp src/hosts_manager.cpp src/parser.cpp
  src/font_loader.cpp src/common.cpp src/i18n.cpp src/log.cpp res/josts.rc)
target_include_directories(Josts PRIVATE src res)
target_link_libraries(Josts PRIVATE comctl32 comdlg32 gdi32 shell32 ws2_32 advapi32 bcrypt ole32)
add_custom_command(TARGET Josts POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/hosts.txt" "$<TARGET_FILE_DIR:Josts>/hosts.txt"
  COMMENT "Copiando la precarga junto a Josts.exe")
if(MSVC)
  set_property(TARGET Josts PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(Josts PRIVATE /W4 /utf-8 /EHsc)
  target_link_options(Josts PRIVATE /MANIFEST:NO)
elseif(MINGW)
  target_compile_options(Josts PRIVATE -Wall -Wextra -municode)
  target_link_options(Josts PRIVATE -static -static-libgcc -static-libstdc++ -municode)
endif()
option(JOSTS_BUILD_TESTS "Compilar pruebas de parsing y escritura de hosts" OFF)
if(JOSTS_BUILD_TESTS)
  enable_testing()
  add_executable(josts_core_tests tests/core_tests.cpp src/hosts_manager.cpp src/parser.cpp src/common.cpp src/i18n.cpp src/log.cpp src/font_loader.cpp)
  add_dependencies(josts_core_tests Josts)
  target_include_directories(josts_core_tests PRIVATE src)
  target_link_libraries(josts_core_tests PRIVATE ws2_32 advapi32 bcrypt gdi32)
  if(MSVC)
    set_property(TARGET josts_core_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(josts_core_tests PRIVATE /utf-8 /EHsc /UNDEBUG)
  elseif(MINGW)
    target_compile_options(josts_core_tests PRIVATE -UNDEBUG)
    target_link_options(josts_core_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME core COMMAND josts_core_tests)
  add_executable(josts_ui_tests tests/ui_tests.cpp src/ui.cpp src/privileged_ops.cpp src/edge_settings.cpp src/hosts_manager.cpp src/parser.cpp
    src/common.cpp src/i18n.cpp src/log.cpp src/font_loader.cpp res/josts.rc)
  target_compile_definitions(josts_ui_tests PRIVATE JOSTS_UI_TESTS)
  target_include_directories(josts_ui_tests PRIVATE src res)
  target_link_libraries(josts_ui_tests PRIVATE comctl32 comdlg32 gdi32 shell32 ws2_32 advapi32 bcrypt ole32)
  if(MSVC)
    set_property(TARGET josts_ui_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(josts_ui_tests PRIVATE /W4 /utf-8 /EHsc /UNDEBUG)
    target_link_options(josts_ui_tests PRIVATE /MANIFEST:NO)
  elseif(MINGW)
    target_compile_options(josts_ui_tests PRIVATE -Wall -Wextra -UNDEBUG)
    target_link_options(josts_ui_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME ui COMMAND josts_ui_tests)
  set_tests_properties(ui PROPERTIES TIMEOUT 60)
  add_executable(josts_edge_tests tests/edge_settings_tests.cpp)
  if(MSVC)
    set_property(TARGET josts_edge_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(josts_edge_tests PRIVATE /W4 /utf-8 /EHsc /UNDEBUG)
  elseif(MINGW)
    target_compile_options(josts_edge_tests PRIVATE -Wall -Wextra -UNDEBUG)
    target_link_options(josts_edge_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME edge_policy COMMAND josts_edge_tests)
  add_executable(josts_privileged_tests tests/privileged_tests.cpp src/privileged_ops.cpp src/hosts_manager.cpp
    src/edge_settings.cpp src/parser.cpp src/common.cpp src/i18n.cpp src/log.cpp)
  target_compile_definitions(josts_privileged_tests PRIVATE JOSTS_BROKER_TESTS)
  target_include_directories(josts_privileged_tests PRIVATE src)
  target_link_libraries(josts_privileged_tests PRIVATE shell32 ws2_32 advapi32 bcrypt ole32)
  if(MSVC)
    set_property(TARGET josts_privileged_tests PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(josts_privileged_tests PRIVATE /W4 /utf-8 /EHsc /UNDEBUG)
  else()
    target_compile_options(josts_privileged_tests PRIVATE -Wall -Wextra -UNDEBUG)
    target_link_options(josts_privileged_tests PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
  add_test(NAME privileged COMMAND josts_privileged_tests)
  set_tests_properties(privileged PROPERTIES TIMEOUT 90)
endif()
