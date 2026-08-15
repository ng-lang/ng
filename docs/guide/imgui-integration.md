# ImGui Integration

NG binds [Dear ImGui](https://github.com/ocornut/imgui) over the SDL3 GPU
backend. The binding is a set of `native fun`s registered by the
`ngi_imgui` frontend (`lib/std/imgui.ng`); under plain `ngi` they report
an unregistered native.

## The frame loop

The NG program drives the loop itself:

```ng
import prelude;
import imgui;

fun main() -> unit {
    imguiInit();
    loop (frame = 0) {
        imguiEventLoop();
        imguiNewFrame();

        imguiBegin("Hello");
        imguiTextWrapped("immediate-mode from NG");
        if (imguiButton("Click me")) {
            print("clicked");
        }
        imguiEnd();

        imguiRender();
        if (not(imguiAborted())) { next (frame + 1); }
    }
    imguiCleanup();
}
```

Run with:

```bash
./build/ngi_imgui example/hello_gui.ng --fuel 0
```

`--fuel 0` lifts the instruction budget — interactive loops run until the
window closes.

## Binding surface

- Lifecycle: `imguiInit`, `imguiCleanup`, `imguiEventLoop`,
  `imguiNewFrame`, `imguiRender`, `imguiAborted`
- Windows: `imguiBegin`, `imguiEnd`, `imguiSetNextWindowSize`,
  `imguiBeginChild`, `imguiEndChild`
- Layout/text: `imguiText`, `imguiTextWrapped`, `imguiSeparator`
- Widgets: `imguiButton`, `imguiCheckbox`,
  `imguiInputTextMultiline` (returns the edited string — rebind it:
  `source := imguiInputTextMultiline("##s", source, w, h);`)
- Style/queries: `imguiStyleColorsDark`, `imguiStyleColorsLight`,
  `imguiGetTime`

## The NG IDE

`example/ng_ide.ng` is a minimal IDE written in NG on this binding: a
multiline editor, Run/Reset buttons, a theme toggle, and an output panel.
Run compiles the editor text through the real pipeline via the prelude's
`runNgi` native, so diagnostics (type errors included) land in the output
panel:

```bash
./build/ngi_imgui example/ng_ide.ng --fuel 0
```

The IDE is also exercised headless in the test suite through stub imgui
natives (`test/imgui_binding_test.cpp`).
