# FarmEye motor katmanı (GStreamer / TensorRT) — ana CMake'e opsiyonel dahil
# Kullanım: ana CMakeLists.txt içinde:
#   add_subdirectory(src/engine)  # veya include(src/engine/Engine.cmake)

option(FARMEYE_ENABLE_GSTREAMER "GStreamer ile kamera yakalama" ON)
option(FARMEYE_ENABLE_TENSORRT "TensorRT inference" OFF)

set(FARMEYE_ENGINE_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/GstCameraCapture.cpp
    ${CMAKE_CURRENT_LIST_DIR}/InferenceEngine.cpp
)

set(FARMEYE_ENGINE_HEADERS
    ${CMAKE_CURRENT_LIST_DIR}/GstCameraCapture.h
    ${CMAKE_CURRENT_LIST_DIR}/InferenceEngine.h
)

if(FARMEYE_ENABLE_GSTREAMER)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GST QUIET gstreamer-1.0 gstreamer-app-1.0)
    endif()

    if(NOT GST_FOUND)
        if(DEFINED ENV{LOCALAPPDATA})
            set(_gst_user "$ENV{LOCALAPPDATA}/Programs/gstreamer/1.0/msvc_x86_64")
            if(EXISTS "${_gst_user}/include/gstreamer-1.0/gst/gst.h")
                set(GSTREAMER_ROOT "${_gst_user}")
            endif()
        endif()
        set(_gst_candidates
            "C:/Program Files/gstreamer/1.0/msvc_x86_64"
            "C:/gstreamer/1.0/msvc_x86_64"
        )
        foreach(_root IN LISTS _gst_candidates)
            if(NOT GSTREAMER_ROOT AND EXISTS "${_root}/include/gstreamer-1.0/gst/gst.h")
                set(GSTREAMER_ROOT "${_root}")
            endif()
        endforeach()
        if(GSTREAMER_ROOT)
            set(GST_INCLUDE_DIRS
                "${GSTREAMER_ROOT}/include/gstreamer-1.0"
                "${GSTREAMER_ROOT}/include/glib-2.0"
                "${GSTREAMER_ROOT}/lib/glib-2.0/include"
            )
            set(GST_LIBRARY_DIRS "${GSTREAMER_ROOT}/lib")
            set(GST_LIBRARIES gstreamer-1.0 gstapp-1.0 gobject-2.0 glib-2.0)
            set(GST_FOUND TRUE)
        endif()
    endif()

    if(GST_FOUND)
        add_compile_definitions(HAVE_GSTREAMER=1)
        include_directories(${GST_INCLUDE_DIRS})
        link_directories(${GST_LIBRARY_DIRS})
        message(STATUS "FarmEye: GStreamer FOUND")
    else()
        message(WARNING "FarmEye: GStreamer NOT found — GstCameraCapture stub modunda")
    endif()
endif()
