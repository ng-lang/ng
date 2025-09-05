

# region IMGUI
set(IMGUI_SRC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_tables.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_demo.cpp
        imgui/backends/imgui_impl_sdl3.cpp
        imgui/backends/imgui_impl_sdlgpu3.cpp
    )

add_library(imgui ${IMGUI_SRC})

target_include_directories(imgui PUBLIC imgui imgui/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

add_executable(imgui_demo
        imgui/examples/example_sdl3_sdlgpu3/main.cpp
    )

target_link_libraries(imgui_demo PRIVATE imgui SDL3::SDL3-static)

# endregion IMGUI