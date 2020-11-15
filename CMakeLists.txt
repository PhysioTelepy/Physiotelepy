cmake_minimum_required(VERSION 2.8)

set(PROJECT_NAME Physiotelepy)
project(${PROJECT_NAME})

add_definitions(-std=c++11 -D_GLIBCXX_USE_CXX11_ABI=0)

set(NUITRACK_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/Dependencies/Nuitrack/include ${CMAKE_CURRENT_LIST_DIR}/Dependencies/Nuitrack/include/middleware)
set(GLFW_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/Dependencies/GLFW/include)
set(GLAD_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/Dependencies/Glad/include)

if(UNIX)
	set(OPENGL_LIBS glut GL)
	IF (CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
		set(PLATFORM_DIR linux_arm)
	ELSEIF(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
		set(PLATFORM_DIR linux64)
	ENDIF()
elseif(WIN32)
	IF(CMAKE_SIZEOF_VOID_P EQUAL 4)
		set(PLATFORM_DIR win32)
	ELSEIF(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(PLATFORM_DIR win64)
	ENDIF()
	set(OPENGL_LIBS glfw3 opengl32 User32 Gdi32 Shell32)
endif()

set(NUITRACK_LIBRARY_PATH ${CMAKE_CURRENT_LIST_DIR}/Dependencies/Nuitrack/lib/${PLATFORM_DIR}) 
set(GLFW_LIBRARY_PATH ${CMAKE_CURRENT_LIST_DIR}/Dependencies/GLFW/lib-vc2019/${PLATFORM_DIR})  

include_directories(
    .
    ${NUITRACK_INCLUDE_PATH}
    ${GLFW_INCLUDE_PATH}
    ${GLAD_INCLUDE_PATH}
)

link_directories(
    ${NUITRACK_LIBRARY_PATH}
    ${GLFW_LIBRARY_PATH}
)

set(SOURCES
    src/NuitrackGLSample.cpp
    src/Application.cpp
    src/NuitrackGLSample.h
    src/UserInteraction.h
    src/UserInteraction.cpp
    src/opgl.h
    src/opgl.cpp
    src/imgui/imconfig.h
    src/imgui/imgui.cpp
    src/imgui/imgui.h
    src/imgui/imgui_demo.cpp
    src/imgui/imgui_draw.cpp
    src/imgui/imgui_internal.h
    src/imgui/imgui_widgets.cpp
    src/imgui/imstb_rectpack.h
    src/imgui/imstb_textedit.h
    src/imgui/imstb_truetype.h
    src/imgui/imgui_impl_opengl3.cpp
    src/imgui/imgui_impl_opengl3.h
    src/imgui/imgui_impl_glfw.cpp
    src/imgui/imgui_impl_glfw.h
    src/glad/glad.c
)
    
add_executable(${PROJECT_NAME} ${SOURCES})

set(LIBS ${OPENGL_LIBS} nuitrack) 

target_link_libraries(${PROJECT_NAME} ${LIBS})
