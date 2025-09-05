
project(imgui)

set(IMGUI_BASE_DIR imgui@v1.92.2b)

# region IMGUI
set(IMGUI_SRC
        ${IMGUI_BASE_DIR}/imgui.cpp
        ${IMGUI_BASE_DIR}/imgui_draw.cpp
        ${IMGUI_BASE_DIR}/imgui_tables.cpp
        ${IMGUI_BASE_DIR}/imgui_widgets.cpp
        ${IMGUI_BASE_DIR}/imgui_demo.cpp
        ${IMGUI_BASE_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_BASE_DIR}/backends/imgui_impl_sdlgpu3.cpp
    )

add_library(imgui ${IMGUI_SRC})

target_include_directories(imgui PUBLIC ${IMGUI_BASE_DIR} ${IMGUI_BASE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

# endregion IMGUI