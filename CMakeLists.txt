cmake_minimum_required(VERSION 3.16)
project(FarmEyeAI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FARMEYE_BUILD_ENGINE "Build farmeye_engine console" ON)
option(FARMEYE_BUILD_UI "Build Qt FarmEyeAI UI" ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui dnn videoio)
include(${CMAKE_CURRENT_SOURCE_DIR}/src/engine/Engine.cmake)

set(ENGINE_COMMON
    src/engine/GstCameraCapture.cpp
    src/engine/InferenceEngine.cpp
    src/engine/preprocess.cpp
    src/engine/class_policy.cpp
    src/tracking/tracker.cpp
)

if(FARMEYE_BUILD_ENGINE)
    add_executable(farmeye_engine
        src/engine/engine_main.cpp
        ${ENGINE_COMMON}
    )
    target_include_directories(farmeye_engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(farmeye_engine PRIVATE ${OpenCV_LIBS})
    target_compile_definitions(farmeye_engine PRIVATE FARMEYE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    if(GST_FOUND)
        target_include_directories(farmeye_engine PRIVATE ${GST_INCLUDE_DIRS})
        target_link_directories(farmeye_engine PRIVATE ${GST_LIBRARY_DIRS})
        target_link_libraries(farmeye_engine PRIVATE ${GST_LIBRARIES})
        target_compile_definitions(farmeye_engine PRIVATE HAVE_GSTREAMER=1)
    endif()
endif()

if(FARMEYE_BUILD_UI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets Gui Sql)

    add_executable(FarmEyeAI
        src/main.cpp
        ${ENGINE_COMMON}
        src/app/pipeline_controller.cpp
        src/app/frame_ring_buffer.cpp
        src/app/system_monitor.cpp
        src/app/person_identity_store.cpp
        src/app/farmeye_store.cpp
        src/app/behavior_analyzer.cpp
        src/ui/mainwindow.cpp
        src/ui/video_gl_widget.cpp
        src/ui/alert_list_panel.cpp
        src/ui/camera_grid_panel.cpp
        src/ui/system_status_bar.cpp
        src/ui/clip_player_dialog.cpp
        src/ui/sparkline_widget.cpp
        src/ui/mainwindow.h
        src/ui/video_gl_widget.h
        src/ui/alert_list_panel.h
        src/ui/camera_grid_panel.h
        src/ui/system_status_bar.h
        src/ui/clip_player_dialog.h
        src/ui/sparkline_widget.h
        src/app/pipeline_controller.h
        src/app/frame_ring_buffer.h
        src/app/system_monitor.h
        src/app/person_identity_store.h
        src/app/farmeye_store.h
        src/app/behavior_analyzer.h
        src/app/farmeye_paths.h
        resources/app.qrc
    )

    target_include_directories(FarmEyeAI PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OpenCV_INCLUDE_DIRS}
    )
    target_link_libraries(FarmEyeAI PRIVATE
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Gui
        Qt6::Sql
        ${OpenCV_LIBS}
    )
    if(WIN32)
        target_link_libraries(FarmEyeAI PRIVATE opengl32)
    endif()
    target_compile_definitions(FarmEyeAI PRIVATE FARMEYE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

    if(GST_FOUND)
        target_include_directories(FarmEyeAI PRIVATE ${GST_INCLUDE_DIRS})
        target_link_directories(FarmEyeAI PRIVATE ${GST_LIBRARY_DIRS})
        target_link_libraries(FarmEyeAI PRIVATE ${GST_LIBRARIES})
        target_compile_definitions(FarmEyeAI PRIVATE HAVE_GSTREAMER=1)
    endif()

    # theme.qss as resource
    message(STATUS "FarmEyeAI UI enabled (Qt6 + OpenCV)")
endif()
