cmake_minimum_required(VERSION 3.13.0)
project(app)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SUPPRESS_REGENERATION true)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 20)				# We need some C++ to interface with DXC, but we wrap it using C
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS ON)

file(GLOB_RECURSE app_android_SRC
	"*.cpp"
	"${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c"
)

add_library(
	app SHARED
	${app_android_SRC}
)

# target_include_directories(${TARGET_NAME} PUBLIC "$ENV{VULKAN_SDK}/include")
# target_link_directories(${TARGET_NAME} PUBLIC "$ENV{VULKAN_SDK}/lib")

target_include_directories(app PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
target_link_libraries(app android log vulkan)
