// AI-generated code; reviewed for this repository's vNext rewrite.
#include "imgui_natives.hpp"

#include "bytecode.hpp"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>

namespace NG
{
  namespace
  {
    struct ImGuiModuleState
    {
      SDL_Window *window = nullptr;
      SDL_GPUDevice *gpuDevice = nullptr;
      SDL_Event event{};
      bool done = false;
      bool sdlInitialized = false;
      bool windowClaimed = false;
      bool imguiInitialized = false;

      ~ImGuiModuleState() { shutdown(); }

      void shutdown()
      {
        if (gpuDevice != nullptr)
        {
          SDL_WaitForGPUIdle(gpuDevice);
        }
        if (imguiInitialized)
        {
          ImGui_ImplSDL3_Shutdown();
          ImGui_ImplSDLGPU3_Shutdown();
          if (ImGui::GetCurrentContext() != nullptr)
          {
            ImGui::DestroyContext();
          }
          imguiInitialized = false;
        }
        if (windowClaimed && gpuDevice != nullptr && window != nullptr)
        {
          SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
          windowClaimed = false;
        }
        if (gpuDevice != nullptr)
        {
          SDL_DestroyGPUDevice(gpuDevice);
          gpuDevice = nullptr;
        }
        if (window != nullptr)
        {
          SDL_DestroyWindow(window);
          window = nullptr;
        }
        if (sdlInitialized)
        {
          SDL_Quit();
          sdlInitialized = false;
        }
        done = false;
      }
    };

    auto activeState() -> std::shared_ptr<ImGuiModuleState> &
    {
      static std::shared_ptr<ImGuiModuleState> state;
      return state;
    }

    auto requireState(const std::string &functionName) -> std::shared_ptr<ImGuiModuleState>
    {
      auto state = activeState();
      if (!state)
      {
        throw bytecode::BytecodeError(std::format("{}() requires imguiInit() before use", functionName));
      }
      return state;
    }

    auto expectString(const std::vector<Value> &arguments, size_t index, const std::string &functionName)
        -> const std::string &
    {
      if (arguments.size() <= index || !arguments[index].isString())
        throw bytecode::BytecodeError(std::format("{}() expects a string at argument {}", functionName, index + 1));
      return arguments[index].asString();
    }

    auto expectI64(const std::vector<Value> &arguments, size_t index, const std::string &functionName) -> int64_t
    {
      if (arguments.size() <= index || !arguments[index].isInteger())
        throw bytecode::BytecodeError(std::format("{}() expects an integer at argument {}", functionName, index + 1));
      return arguments[index].asInteger();
    }

    auto expectF32(const std::vector<Value> &arguments, size_t index, const std::string &functionName) -> float
    {
      if (arguments.size() <= index)
        throw bytecode::BytecodeError(std::format("{}() expects an f32 at argument {}", functionName, index + 1));
      return static_cast<float>(arguments[index].isInteger() ? arguments[index].asInteger() : arguments[index].asDouble());
    }

    auto expectBool(const std::vector<Value> &arguments, size_t index, const std::string &functionName) -> bool
    {
      if (arguments.size() <= index || !arguments[index].isInteger())
        throw bytecode::BytecodeError(std::format("{}() expects a bool at argument {}", functionName, index + 1));
      return arguments[index].asInteger() != 0;
    }

    auto boolValue(bool value) -> Value { return Value::integer(value ? 1 : 0); }

    void addFontOrThrow(ImGuiIO &io, const std::filesystem::path &fontPath)
    {
      if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 0.0F, nullptr, nullptr) == nullptr)
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): failed to load font `{}`", fontPath.string()));
      }
    }

    auto runtimeFontPath(const std::filesystem::path &relativePath) -> std::filesystem::path
    {
      namespace fs = std::filesystem;
      const auto sourcePath = [&] {
        auto path = fs::path(__FILE__);
        if (path.is_relative()) path = fs::current_path() / path;
        return path.lexically_normal();
      }();
      const std::vector<fs::path> candidates{
          fs::current_path() / relativePath,
          fs::current_path().parent_path() / relativePath,
          sourcePath.parent_path().parent_path().parent_path() / relativePath,
      };
      for (const auto &candidate : candidates)
      {
        if (fs::exists(candidate)) return candidate;
      }
      throw bytecode::BytecodeError(std::format("imguiInit(): unable to locate runtime asset `{}`", relativePath.string()));
    }

    auto imguiInit(const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) -> Value
    {
      if (arguments.size() != 0) throw bytecode::BytecodeError("imguiInit() takes no arguments");
      if (activeState()) throw bytecode::BytecodeError("imguiInit() called while an imgui state is still active");
      auto state = std::make_shared<ImGuiModuleState>();
      if (!SDL_Init(SDL_INIT_VIDEO))
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): SDL_Init() failed: {}", SDL_GetError()));
      }
      state->sdlInitialized = true;

      const float mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
      const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
      state->window = SDL_CreateWindow("NG IDE", static_cast<int>(1180 * mainScale), static_cast<int>(760 * mainScale),
                                       windowFlags);
      if (state->window == nullptr)
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): SDL_CreateWindow() failed: {}", SDL_GetError()));
      }
      SDL_SetWindowPosition(state->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
      SDL_ShowWindow(state->window);

      state->gpuDevice = SDL_CreateGPUDevice(
          SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
      if (state->gpuDevice == nullptr)
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): SDL_CreateGPUDevice() failed: {}", SDL_GetError()));
      }
      if (!SDL_ClaimWindowForGPUDevice(state->gpuDevice, state->window))
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): SDL_ClaimWindowForGPUDevice() failed: {}", SDL_GetError()));
      }
      state->windowClaimed = true;
      SDL_SetGPUSwapchainParameters(state->gpuDevice, state->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                    SDL_GPU_PRESENTMODE_VSYNC);

      IMGUI_CHECKVERSION();
      ImGui::CreateContext();
      state->imguiInitialized = true;

      ImGuiIO &io = ImGui::GetIO();
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
      ImGui::StyleColorsLight();
      ImGuiStyle &style = ImGui::GetStyle();
      style.ScaleAllSizes(mainScale);
      style.FontScaleDpi = mainScale;

      if (!ImGui_ImplSDL3_InitForSDLGPU(state->window))
      {
        throw bytecode::BytecodeError(std::format("imguiInit(): SDL3 backend init failed: {}", SDL_GetError()));
      }
      ImGui_ImplSDLGPU3_InitInfo initInfo = {};
      initInfo.Device = state->gpuDevice;
      initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state->gpuDevice, state->window);
      initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
      if (!ImGui_ImplSDLGPU3_Init(&initInfo))
      {
        throw bytecode::BytecodeError("imguiInit(): SDLGPU3 backend init failed");
      }

      addFontOrThrow(io, runtimeFontPath("misc/fonts/SourceSans/SourceSans3-Regular.otf"));
      addFontOrThrow(io, runtimeFontPath("misc/fonts/SourceCodePro/SourceCodePro-Regular.otf"));

      activeState() = state;
      return Value{};
    }

    auto imguiCleanup(const std::vector<Value> &, const std::vector<typecheck::TypeId> &) -> Value
    {
      if (auto state = activeState())
      {
        state->shutdown();
        activeState().reset();
      }
      return Value{};
    }
  } // namespace

  void registerImguiNatives(vm::NativeRegistry &registry)
  {
    registry.registerNative("imguiInit", imguiInit);
    registry.registerNative("imguiCleanup", imguiCleanup);

    registry.registerNative("imguiEventLoop", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      auto state = requireState("imguiEventLoop");
      while (SDL_PollEvent(&state->event))
      {
        ImGui_ImplSDL3_ProcessEvent(&state->event);
        if (state->event.type == SDL_EVENT_QUIT)
        {
          state->done = true;
        }
        if (state->event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
            state->event.window.windowID == SDL_GetWindowID(state->window))
        {
          state->done = true;
        }
      }
      return Value{};
    });

    registry.registerNative("imguiNewFrame", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      requireState("imguiNewFrame");
      ImGui_ImplSDLGPU3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();
      return Value{};
    });

    registry.registerNative("imguiRender", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      auto state = requireState("imguiRender");
      ImGui::Render();

      ImDrawData *drawData = ImGui::GetDrawData();
      const bool minimized = (drawData->DisplaySize.x <= 0.0F || drawData->DisplaySize.y <= 0.0F);

      SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(state->gpuDevice);
      if (commandBuffer == nullptr)
      {
        SDL_Log("imguiRender(): SDL_AcquireGPUCommandBuffer() failed: %s", SDL_GetError());
        return Value{};
      }

      SDL_GPUTexture *swapchainTexture = nullptr;
      if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, state->window, &swapchainTexture, nullptr, nullptr))
      {
        SDL_SubmitGPUCommandBuffer(commandBuffer);
        return Value{};
      }

      if (swapchainTexture != nullptr)
      {
        SDL_GPUColorTargetInfo colorTarget = {};
        colorTarget.texture = swapchainTexture;
        colorTarget.clear_color = SDL_FColor{0.45F, 0.55F, 0.60F, 1.00F};
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;
        ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);
        SDL_GPURenderPass *renderPass = SDL_BeginGPURenderPass(commandBuffer, &colorTarget, 1, nullptr);
        ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, renderPass, nullptr);
        SDL_EndGPURenderPass(renderPass);
      }
      static_cast<void>(minimized);
      SDL_SubmitGPUCommandBuffer(commandBuffer);
      return Value{};
    });

    registry.registerNative("imguiAborted", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      auto state = requireState("imguiAborted");
      return boolValue(state->done);
    });

    registry.registerNative("imguiBegin", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      requireState("imguiBegin");
      return boolValue(ImGui::Begin(expectString(arguments, 0, "imguiBegin").c_str()));
    });

    registry.registerNative("imguiEnd", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      requireState("imguiEnd");
      ImGui::End();
      return Value{};
    });

    registry.registerNative("imguiSetNextWindowSize",
                            [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiSetNextWindowSize");
                              const float width = expectF32(arguments, 0, "imguiSetNextWindowSize");
                              const float height = expectF32(arguments, 1, "imguiSetNextWindowSize");
                              ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_FirstUseEver);
                              return Value{};
                            });

    registry.registerNative("imguiText", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      requireState("imguiText");
      ImGui::TextUnformatted(expectString(arguments, 0, "imguiText").c_str());
      return Value{};
    });

    registry.registerNative("imguiTextWrapped",
                            [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiTextWrapped");
                              ImGui::TextWrapped("%s", expectString(arguments, 0, "imguiTextWrapped").c_str());
                              return Value{};
                            });

    registry.registerNative("imguiSeparator", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      requireState("imguiSeparator");
      ImGui::Separator();
      return Value{};
    });

    registry.registerNative("imguiButton", [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
      requireState("imguiButton");
      return boolValue(ImGui::Button(expectString(arguments, 0, "imguiButton").c_str()));
    });

    registry.registerNative("imguiCheckbox",
                            [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiCheckbox");
                              bool checked = expectBool(arguments, 1, "imguiCheckbox");
                              ImGui::Checkbox(expectString(arguments, 0, "imguiCheckbox").c_str(), &checked);
                              return boolValue(checked);
                            });

    registry.registerNative("imguiBeginChild",
                            [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiBeginChild");
                              const float width = expectF32(arguments, 1, "imguiBeginChild");
                              const float height = expectF32(arguments, 2, "imguiBeginChild");
                              return boolValue(ImGui::BeginChild(expectString(arguments, 0, "imguiBeginChild").c_str(),
                                                                 ImVec2(width, height), ImGuiChildFlags_Border));
                            });

    registry.registerNative("imguiEndChild", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      requireState("imguiEndChild");
      ImGui::EndChild();
      return Value{};
    });

    registry.registerNative("imguiInputTextMultiline",
                            [](const std::vector<Value> &arguments, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiInputTextMultiline");
                              const auto &label = expectString(arguments, 0, "imguiInputTextMultiline");
                              const auto &value = expectString(arguments, 1, "imguiInputTextMultiline");
                              const float width = expectF32(arguments, 2, "imguiInputTextMultiline");
                              const float height = expectF32(arguments, 3, "imguiInputTextMultiline");
                              const size_t bufferSize = std::max<size_t>(64 * 1024, value.size() + 4096);
                              std::string buffer(bufferSize, '\0');
                              value.copy(buffer.data(), std::min(value.size(), buffer.size() - 1));
                              ImGui::InputTextMultiline(label.c_str(), buffer.data(), buffer.size(),
                                                        ImVec2(width, height));
                              const auto end = buffer.find('\0');
                              return Value::string(buffer.substr(0, end));
                            });

    registry.registerNative("imguiStyleColorsDark",
                            [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiStyleColorsDark");
                              ImGui::StyleColorsDark();
                              return Value{};
                            });

    registry.registerNative("imguiStyleColorsLight",
                            [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
                              requireState("imguiStyleColorsLight");
                              ImGui::StyleColorsLight();
                              return Value{};
                            });

    registry.registerNative("imguiGetTime", [](const std::vector<Value> &, const std::vector<typecheck::TypeId> &) {
      requireState("imguiGetTime");
      return Value::float_(static_cast<double>(ImGui::GetTime()));
    });
  }
} // namespace NG
