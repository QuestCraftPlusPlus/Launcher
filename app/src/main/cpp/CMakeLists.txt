# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

project("qcxr")

find_package(OpenXR REQUIRED CONFIG)

include(FetchContent)

FetchContent_Declare(
    VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.3.0
)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

set(KTX_FEATURE_VULKAN ON CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.2
)
FetchContent_MakeAvailable(ktx)


add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ multiSource files with relative paths to this CMakeLists.txt.
        main.c
        xr_init.c
        xr_render.c
        xr_input.c
        vk_init.c
        lightmap_model_gles_program.c
        rendertarget_blit_program.c
        singlecolor_program.c
        ktx_texture.c
        multiview_detect.c
        asset_buffer_read.c
        renderer.c)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        ktx_read
        OpenXR::openxr_loader
        vulkan-wrapper
        VulkanMemoryAllocator)