# ImGui Integration

NG integrates with [Dear ImGui](https://github.com/ocornut/imgui) for immediate-mode GUI programming. This enables creating interactive graphical applications directly in NG.

## Overview

The ImGui binding provides:

- Window and widget creation
- Mouse and keyboard input handling
- Drawing primitives
- Integration with SDL3 for window management

## Running ImGui Examples

```bash
# Build the demo application
cmake --build build -j

# Run the NG ImGui example
./build/ngi example/imgui_example.ng

# Run the NG IDE demo
./build/ngi example/ng_ide.ng
```

## Basic Example

```ng
import std.imgui;

// ImGuiExample.ng
fun main() {
    val show_demo_window = true;
    val show_another_window = false;
    val clear_color = (0.45f32, 0.55f32, 0.60f32, 1.00f32);

    // Create and run the ImGui application loop
    runImGui("NG ImGui Example", {
        // Main menu bar
        if (beginMenuBar()) {
            if (beginMenu("File")) {
                if (menuItem("Exit")) {
                    closeImGui();
                }
                endMenu();
            }
            endMenuBar();
        }

        // Demo window
        showDemoWindow(ref show_demo_window);

        // Custom window
        if (begin("Hello, NG!")) {
            text("This is an ImGui window from NG!");
            text("Counter:");
            sameLine();
            button("Click me");
            end();
        }
    });
}
```

## Available Functions

The `std.imgui` module exposes a comprehensive subset of ImGui's API:

### Window Management

| Function | Description |
|---|---|
| `begin(name: string, p_open: ref<bool>) -> bool` | Start a window |
| `end()` | End a window |
| `beginChild(id: string) -> bool` | Start a child window |
| `endChild()` | End a child window |
| `setNextWindowSize(size: (f32, f32))` | Set next window size |
| `setNextWindowPos(pos: (f32, f32))` | Set next window position |

### Widgets

| Function | Description |
|---|---|
| `text(fmt: string)` | Display text |
| `button(label: string) -> bool` | Clickable button |
| `checkbox(label: string, v: ref<bool>) -> bool` | Checkbox |
| `sliderFloat(label: string, v: ref<f32>, min: f32, max: f32) -> bool` | Float slider |
| `sliderInt(label: string, v: ref<i32>, min: i32, max: i32) -> bool` | Int slider |
| `inputText(label: string, buf: ref<string>) -> bool` | Text input |
| `combo(label: string, current: ref<i32>, items: [string]) -> bool` | Combo box |
| `colorEdit3(label: string, col: ref<(f32, f32, f32)>) -> bool` | Color picker |

### Layout

| Function | Description |
|---|---|
| `sameLine()` | Place next widget on same line |
| `spacing()` | Add spacing |
| `separator()` | Add separator line |
| `indent()` | Indent widgets |
| `unindent()` | Unindent widgets |
| `group()` | Group widgets |
| `dummy(size: (f32, f32))` | Add dummy space |

### Menus

| Function | Description |
|---|---|
| `beginMenuBar() -> bool` | Start menu bar |
| `endMenuBar()` | End menu bar |
| `beginMenu(label: string) -> bool` | Start menu |
| `endMenu()` | End menu |
| `menuItem(label: string) -> bool` | Menu item |

### Drawing

| Function | Description |
|---|---|
| `drawLine(p1: (f32, f32), p2: (f32, f32), col: u32)` | Draw a line |
| `drawRect(p1: (f32, f32), p2: (f32, f32), col: u32)` | Draw a rectangle outline |
| `drawRectFilled(p1: (f32, f32), p2: (f32, f32), col: u32)` | Draw a filled rectangle |
| `drawCircle(center: (f32, f32), radius: f32, col: u32)` | Draw a circle |
| `drawText(pos: (f32, f32), text: string, col: u32)` | Draw text |

### Application Control

| Function | Description |
|---|---|
| `runImGui(title: string, frame_fn: () -> unit)` | Run the main loop |
| `closeImGui()` | Request application exit |
| `showDemoWindow(p_open: ref<bool>)` | Show ImGui demo window |

## Complete Example: Color Picker

```ng
import std.imgui;

fun main() {
    val bg_color = (0.1f32, 0.2f32, 0.3f32, 1.0f32);

    runImGui("Color Picker", {
        if (begin("Settings")) {
            text("Background Color:");
            colorEdit3("Color", ref bg_color);

            if (button("Reset")) {
                bg_color = (0.1f32, 0.2f32, 0.3f32, 1.0f32);
            }

            end();
        }

        // Use bg_color for clear color
        setClearColor(bg_color.0, bg_color.1, bg_color.2, bg_color.3);
    });
}
```

## The NG IDE

The repository includes a full NG IDE built with ImGui (`example/ng_ide.ng`):

```bash
./build/ngi example/ng_ide.ng
```

This demonstrates:
- Code editing with syntax highlighting
- File loading and saving
- Integrated NG execution
- Multiple dockable windows

## ImGui Demo Application

There's also a standalone C++ test application:

```bash
./build/ng_imgui_demo
```

## Architecture

The ImGui integration works through:

1. **NG types and functions** declared in `lib/std/imgui.ng`
2. **Native C++ implementations** in `src/stdlib/imgui.cpp`
3. **SDL3** for window and input management
4. **ImGui** for immediate-mode GUI rendering

The VM bridges NG function calls to C++ implementations transparently.

## What's Next?

Review the [example programs](https://github.com/ng-lang/ng/tree/main/example) for a complete list of runnable programs, or check the [Language Guide](language_guide.md) for other topics.

> **Try it:** `example/imgui_example.ng` — ImGui basics
> **Try it:** `example/ng_ide.ng` — Full NG IDE