// AI-generated code; reviewed for this repository's vNext rewrite.
//
// libngrt imgui backend: C-ABI implementations of `$ngrt_imgui*` symbols used
// by the QBE native tier. Unlike the old VM-facing `NativeRegistry` binding,
// these are plain libngrt-style functions linked directly into generated
// executables.
#include "ngrt.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <vector>

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
        SDL_WaitForGPUIdle(gpuDevice);
      if (imguiInitialized)
      {
        ImGui_ImplSDL3_Shutdown();
        ImGui_ImplSDLGPU3_Shutdown();
        if (ImGui::GetCurrentContext() != nullptr)
          ImGui::DestroyContext();
        imguiInitialized = false;
      }
      if (windowClaimed && gpuDevice != nullptr && window != nullptr)
        SDL_ReleaseWindowFromGPUDevice(gpuDevice, window);
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

  auto requireState() -> std::shared_ptr<ImGuiModuleState>
  {
    auto state = activeState();
    if (!state)
    {
      std::fprintf(stderr, "imgui: imguiInit() must be called first\n");
      return nullptr;
    }
    return state;
  }

  auto fromNg(const char *text) -> std::string
  {
    const auto length = *reinterpret_cast<const long *>(text);
    return std::string{text + 8, static_cast<size_t>(length)};
  }

  auto toNg(const std::string &text) -> char *
  {
    return ngrt_new_string(text.data(), static_cast<long>(text.size()));
  }

  auto addFontOrThrow(ImGuiIO &io, const std::filesystem::path &fontPath) -> bool
  {
    if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 0.0F, nullptr, nullptr) == nullptr)
    {
      std::fprintf(stderr, "imgui: failed to load font %s\n", fontPath.string().c_str());
      return false;
    }
    return true;
  }

  auto runtimeFontPath(const std::filesystem::path &relativePath) -> std::filesystem::path
  {
    namespace fs = std::filesystem;
    auto sourcePath = fs::path(__FILE__);
    if (sourcePath.is_relative())
      sourcePath = fs::current_path() / sourcePath;
    sourcePath = sourcePath.lexically_normal();
    const std::vector<fs::path> candidates{
        fs::current_path() / relativePath,
        fs::current_path().parent_path() / relativePath,
        sourcePath.parent_path().parent_path().parent_path() / relativePath,
    };
    for (const auto &candidate : candidates)
      if (fs::exists(candidate))
        return candidate;
    return fs::current_path() / relativePath;
  }
} // namespace

extern "C" long ngrt_imguiInit(void)
{
  if (activeState())
  {
    std::fprintf(stderr, "imguiInit: already active\n");
    return 1;
  }
  auto state = std::make_shared<ImGuiModuleState>();
  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::fprintf(stderr, "imguiInit: SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  state->sdlInitialized = true;

  const float mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
  state->window = SDL_CreateWindow("NG IDE", static_cast<int>(1180 * mainScale), static_cast<int>(760 * mainScale),
                                   windowFlags);
  if (state->window == nullptr)
  {
    std::fprintf(stderr, "imguiInit: SDL_CreateWindow failed: %s\n", SDL_GetError());
    state->shutdown();
    return 1;
  }
  SDL_SetWindowPosition(state->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(state->window);

  state->gpuDevice = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
  if (state->gpuDevice == nullptr)
  {
    std::fprintf(stderr, "imguiInit: SDL_CreateGPUDevice failed: %s\n", SDL_GetError());
    state->shutdown();
    return 1;
  }
  if (!SDL_ClaimWindowForGPUDevice(state->gpuDevice, state->window))
  {
    std::fprintf(stderr, "imguiInit: SDL_ClaimWindowForGPUDevice failed: %s\n", SDL_GetError());
    state->shutdown();
    return 1;
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
    std::fprintf(stderr, "imguiInit: SDL3 backend init failed: %s\n", SDL_GetError());
    state->shutdown();
    return 1;
  }
  ImGui_ImplSDLGPU3_InitInfo initInfo = {};
  initInfo.Device = state->gpuDevice;
  initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state->gpuDevice, state->window);
  initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  if (!ImGui_ImplSDLGPU3_Init(&initInfo))
  {
    std::fprintf(stderr, "imguiInit: SDLGPU3 backend init failed\n");
    state->shutdown();
    return 1;
  }

  addFontOrThrow(io, runtimeFontPath("misc/fonts/SourceSans/SourceSans3-Regular.otf"));
  addFontOrThrow(io, runtimeFontPath("misc/fonts/SourceCodePro/SourceCodePro-Regular.otf"));

  activeState() = state;
  return 0;
}

extern "C" long ngrt_imguiCleanup(void)
{
  if (auto state = activeState())
  {
    state->shutdown();
    activeState().reset();
  }
  return 0;
}

extern "C" long ngrt_imguiEventLoop(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  while (SDL_PollEvent(&state->event))
  {
    ImGui_ImplSDL3_ProcessEvent(&state->event);
    if (state->event.type == SDL_EVENT_QUIT)
      state->done = true;
    if (state->event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        state->event.window.windowID == SDL_GetWindowID(state->window))
      state->done = true;
  }
  return 0;
}

extern "C" long ngrt_imguiNewFrame(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  return 0;
}

extern "C" long ngrt_imguiRender(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::Render();
  ImDrawData *drawData = ImGui::GetDrawData();
  SDL_GPUCommandBuffer *commandBuffer = SDL_AcquireGPUCommandBuffer(state->gpuDevice);
  if (commandBuffer == nullptr)
  {
    SDL_Log("imguiRender: SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return 0;
  }
  SDL_GPUTexture *swapchainTexture = nullptr;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(commandBuffer, state->window, &swapchainTexture, nullptr, nullptr))
  {
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return 0;
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
  SDL_SubmitGPUCommandBuffer(commandBuffer);
  return 0;
}

extern "C" long ngrt_imguiAborted(void)
{
  auto state = requireState();
  return state && state->done ? 1 : 0;
}

extern "C" long ngrt_imguiBegin(const char *title)
{
  auto state = requireState();
  if (!state)
    return 0;
  const auto text = fromNg(title);
  return ImGui::Begin(text.c_str()) ? 1 : 0;
}

extern "C" long ngrt_imguiEnd(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::End();
  return 0;
}

extern "C" long ngrt_imguiSetNextWindowSize(double width, double height)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)), ImGuiCond_FirstUseEver);
  return 0;
}

extern "C" long ngrt_imguiBeginChild(const char *id, double width, double height)
{
  auto state = requireState();
  if (!state)
    return 0;
  const auto text = fromNg(id);
  return ImGui::BeginChild(text.c_str(), ImVec2(static_cast<float>(width), static_cast<float>(height)),
                           ImGuiChildFlags_Border)
             ? 1
             : 0;
}

extern "C" long ngrt_imguiEndChild(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::EndChild();
  return 0;
}

extern "C" long ngrt_imguiText(const char *text)
{
  auto state = requireState();
  if (!state)
    return 1;
  const auto value = fromNg(text);
  ImGui::TextUnformatted(value.c_str());
  return 0;
}

extern "C" long ngrt_imguiTextWrapped(const char *text)
{
  auto state = requireState();
  if (!state)
    return 1;
  const auto value = fromNg(text);
  ImGui::TextWrapped("%s", value.c_str());
  return 0;
}

extern "C" long ngrt_imguiSeparator(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::Separator();
  return 0;
}

extern "C" long ngrt_imguiButton(const char *label)
{
  auto state = requireState();
  if (!state)
    return 0;
  const auto text = fromNg(label);
  return ImGui::Button(text.c_str()) ? 1 : 0;
}

extern "C" long ngrt_imguiCheckbox(const char *label, long checked)
{
  auto state = requireState();
  if (!state)
    return 0;
  const auto text = fromNg(label);
  bool value = checked != 0;
  ImGui::Checkbox(text.c_str(), &value);
  return value ? 1 : 0;
}

extern "C" char *ngrt_imguiInputTextMultiline(const char *label, const char *value, double width, double height)
{
  auto state = requireState();
  if (!state)
    return toNg("");
  const auto labelText = fromNg(label);
  const auto valueText = fromNg(value);
  const size_t bufferSize = std::max<size_t>(64 * 1024, valueText.size() + 4096);
  std::string buffer(bufferSize, '\0');
  valueText.copy(buffer.data(), std::min(valueText.size(), buffer.size() - 1));
  ImGui::InputTextMultiline(labelText.c_str(), buffer.data(), buffer.size(),
                            ImVec2(static_cast<float>(width), static_cast<float>(height)));
  const auto end = buffer.find('\0');
  return toNg(buffer.substr(0, end));
}

extern "C" long ngrt_imguiStyleColorsDark(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::StyleColorsDark();
  return 0;
}

extern "C" long ngrt_imguiStyleColorsLight(void)
{
  auto state = requireState();
  if (!state)
    return 1;
  ImGui::StyleColorsLight();
  return 0;
}

extern "C" double ngrt_imguiGetTime(void)
{
  auto state = requireState();
  if (!state)
    return 0.0;
  return ImGui::GetTime();
}
