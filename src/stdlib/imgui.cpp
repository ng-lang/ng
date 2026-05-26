#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <intp/runtime.hpp>
#include <module.hpp>
#include <orgasm/vm.hpp>
#include <runtime/native_marshaling.hpp>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <regex>
#include <sstream>

namespace NG::library::imgui
{
  using namespace NG::runtime;
  using namespace NG::runtime::native;

  namespace
  {
    struct ImGuiModuleState
    {
      SDL_Window *window = nullptr;
      SDL_GPUDevice *gpu_device = nullptr;
      ImVec4 clear_color = ImVec4(0.45F, 0.55F, 0.60F, 1.00F);
      SDL_Event event{};
      bool done = false;
      bool sdl_initialized = false;
      bool window_claimed = false;
      bool imgui_initialized = false;

      ~ImGuiModuleState() { shutdown(); }

      void shutdown()
      {
        if (gpu_device != nullptr)
        {
          SDL_WaitForGPUIdle(gpu_device);
        }
        if (imgui_initialized)
        {
          ImGui_ImplSDL3_Shutdown();
          ImGui_ImplSDLGPU3_Shutdown();
          if (ImGui::GetCurrentContext() != nullptr)
          {
            ImGui::DestroyContext();
          }
          imgui_initialized = false;
        }
        if (window_claimed && gpu_device != nullptr && window != nullptr)
        {
          SDL_ReleaseWindowFromGPUDevice(gpu_device, window);
          window_claimed = false;
        }
        if (gpu_device != nullptr)
        {
          SDL_DestroyGPUDevice(gpu_device);
          gpu_device = nullptr;
        }
        if (window != nullptr)
        {
          SDL_DestroyWindow(window);
          window = nullptr;
        }
        if (sdl_initialized)
        {
          SDL_Quit();
          sdl_initialized = false;
        }
        done = false;
      }
    };

    auto active_imgui_state() -> std::shared_ptr<ImGuiModuleState> &
    {
      static std::shared_ptr<ImGuiModuleState> state;
      return state;
    }

    auto find_imgui_state(const NGEnv &) -> std::shared_ptr<ImGuiModuleState>
    {
      return active_imgui_state();
    }

    auto require_imgui_state(const Str &functionName, const NGEnv &) -> std::shared_ptr<ImGuiModuleState>
    {
      auto state = active_imgui_state();
      if (!state)
      {
        throw RuntimeException(functionName + "() requires imgui.init() before use");
      }
      return state;
    }

    void store_imgui_state(const NGEnv &, const std::shared_ptr<ImGuiModuleState> &state)
    {
      active_imgui_state() = state;
    }

    void clear_imgui_state(const NGEnv &)
    {
      active_imgui_state().reset();
    }

    auto resolve_runtime_asset_path(const std::filesystem::path &relativePath) -> std::filesystem::path
    {
      namespace fs = std::filesystem;

      Vec<fs::path> candidates{
          fs::current_path(),
          fs::current_path().parent_path(),
      };

      auto sourcePath = fs::path(__FILE__);
      if (sourcePath.is_relative())
      {
        sourcePath = fs::current_path() / sourcePath;
      }
      candidates.push_back(sourcePath.lexically_normal().parent_path().parent_path().parent_path());

      for (const auto &root : candidates)
      {
        auto candidate = (root / relativePath).lexically_normal();
        if (fs::exists(candidate))
        {
          return candidate;
        }
      }

      throw RuntimeException("imgui.init(): unable to locate runtime asset: " + relativePath.string());
    }

    void add_font_or_throw(ImGuiIO &io, const std::filesystem::path &fontPath)
    {
      if (io.Fonts->AddFontFromFileTTF(fontPath.string().c_str()) == nullptr)
      {
        throw RuntimeException("imgui.init(): failed to load font: " + fontPath.string());
      }
    }

    auto borrowed_handle(const Str &typeName, const void *ptr) -> RuntimeRef<StorageCell>
    {
      return make_runtime_native_handle_cell(typeName, reinterpret_cast<uintptr_t>(ptr), false,
                                             StorageClass::TEMPORARY);
    }

    template <typename T>
    auto require_handle_arg(const Str &functionName, const NativeArgsView &args, size_t index, const Str &typeName)
        -> T *
    {
      auto slot = require_arg_slot(functionName, args, index, typeName);
      auto handle = runtime_native_handle_value(slot);
      if (handle.typeName != typeName || handle.address == 0)
      {
        throw RuntimeException(functionName + "() requires " + typeName + " at argument " +
                               std::to_string(index + 1));
      }
      return reinterpret_cast<T *>(handle.address);
    }

    auto require_bool_arg(const Str &functionName, const NativeArgsView &args, size_t index) -> bool
    {
      auto slot = require_arg_slot(functionName, args, index, "a bool");
      auto value = runtime_boolean_value(slot);
      if (!value.has_value())
      {
        throw RuntimeException(functionName + "() requires a bool at argument " + std::to_string(index + 1));
      }
      return *value;
    }

    auto require_i32_arg(const Str &functionName, const NativeArgsView &args, size_t index) -> int32_t
    {
      return require_numeric_arg<int32_t>(functionName, args, index, "an i32");
    }

    auto require_f32_arg(const Str &functionName, const NativeArgsView &args, size_t index) -> float
    {
      return require_numeric_arg<float>(functionName, args, index, "an f32");
    }

    auto i32_cell(int32_t value) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<int32_t>(value); }

    auto f32_cell(float value) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<float>(value); }

    auto f64_cell(double value) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<double>(value); }

    auto input_text_buffer(const Str &value) -> Str
    {
      auto size = std::max<size_t>(1024, value.size() + 256);
      Str buffer(size, '\0');
      std::memcpy(buffer.data(), value.c_str(), std::min(value.size(), size - 1));
      return buffer;
    }

    auto multiline_text_buffer(const Str &value) -> Str
    {
      auto size = std::max<size_t>(64 * 1024, value.size() + 4096);
      Str buffer(size, '\0');
      std::memcpy(buffer.data(), value.c_str(), std::min(value.size(), size - 1));
      return buffer;
    }

    auto token_color(const Str &token) -> ImVec4
    {
      static const std::regex keywordPattern{
          R"(^(module|import|exports|fun|type|trait|impl|val|return|if|else|loop|next|switch|case|new|ref|move|const|where|native|delete)$)"};
      static const std::regex typePattern{R"(^(unit|bool|string|i32|u32|i64|u64|f32|f64|Self)$)"};
      static const std::regex numberPattern{R"(^[0-9]+(\.[0-9]+)?(f32|f64)?$)"};
      if (token.starts_with("//")) return ImVec4(0.45F, 0.55F, 0.50F, 1.0F);
      if (token.size() >= 2 && token.front() == '"' && token.back() == '"') return ImVec4(0.62F, 0.42F, 0.22F, 1.0F);
      if (std::regex_match(token, keywordPattern)) return ImVec4(0.22F, 0.34F, 0.70F, 1.0F);
      if (std::regex_match(token, typePattern)) return ImVec4(0.56F, 0.23F, 0.63F, 1.0F);
      if (std::regex_match(token, numberPattern)) return ImVec4(0.16F, 0.48F, 0.52F, 1.0F);
      return ImVec4(0.12F, 0.14F, 0.16F, 1.0F);
    }

    void render_highlighted_ng_source(const Str &source)
    {
      static const std::regex tokenPattern{
          R"(//.*|\"([^\"\\]|\\.)*\"|[A-Za-z_][A-Za-z0-9_]*|[0-9]+(\.[0-9]+)?(f32|f64)?|\S)"};
      std::istringstream lines{source};
      Str line;
      while (std::getline(lines, line))
      {
        bool firstToken = true;
        for (auto it = std::sregex_iterator(line.begin(), line.end(), tokenPattern);
             it != std::sregex_iterator(); ++it)
        {
          auto token = it->str();
          if (!firstToken)
          {
            ImGui::SameLine(0.0F, 3.0F);
          }
          auto color = token_color(token);
          ImGui::TextColored(color, "%s", token.c_str());
          firstToken = false;
          if (token.starts_with("//"))
          {
            break;
          }
        }
        if (firstToken)
        {
          ImGui::TextUnformatted("");
        }
      }
    }
  } // namespace

  static Map<Str, NGCallable> handlers{
      {"GetVersion",
       [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         return make_runtime_string(ImGui::GetVersion());
       }},
      {"init",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         if (find_imgui_state(context))
         {
           throw RuntimeException("imgui.init() called while an imgui state is still active");
         }

         auto state = std::make_shared<ImGuiModuleState>();
         if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
         {
           throw RuntimeException("imgui.init(): SDL_Init() failed: " + Str(SDL_GetError()));
         }
         state->sdl_initialized = true;

         float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
         SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
         state->window = SDL_CreateWindow("Dear ImGui NG Binding + SDL3 backend Example", static_cast<int>(1280 * main_scale),
                                          static_cast<int>(720 * main_scale), window_flags);
         if (state->window == nullptr)
         {
           throw RuntimeException("imgui.init(): SDL_CreateWindow() failed: " + Str(SDL_GetError()));
         }
         SDL_SetWindowPosition(state->window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
         SDL_ShowWindow(state->window);

         state->gpu_device =
             SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
                                 true, nullptr);
         if (state->gpu_device == nullptr)
         {
           throw RuntimeException("imgui.init(): SDL_CreateGPUDevice() failed: " + Str(SDL_GetError()));
         }

         if (!SDL_ClaimWindowForGPUDevice(state->gpu_device, state->window))
         {
           throw RuntimeException("imgui.init(): SDL_ClaimWindowForGPUDevice() failed: " + Str(SDL_GetError()));
         }
         state->window_claimed = true;
         SDL_SetGPUSwapchainParameters(state->gpu_device, state->window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                       SDL_GPU_PRESENTMODE_VSYNC);

         IMGUI_CHECKVERSION();
         ImGui::CreateContext();
         state->imgui_initialized = true;

         ImGuiIO &io = ImGui::GetIO();
         io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
         io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

         ImGui::StyleColorsLight();
         ImGuiStyle &style = ImGui::GetStyle();
         style.ScaleAllSizes(main_scale);
         style.FontScaleDpi = main_scale;

         if (!ImGui_ImplSDL3_InitForSDLGPU(state->window))
         {
           throw RuntimeException("imgui.init(): failed to initialize ImGui SDL3 backend: " + Str(SDL_GetError()));
         }
         ImGui_ImplSDLGPU3_InitInfo init_info = {};
         init_info.Device = state->gpu_device;
         init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(state->gpu_device, state->window);
         init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
         if (!ImGui_ImplSDLGPU3_Init(&init_info))
         {
           throw RuntimeException("imgui.init(): failed to initialize ImGui SDLGPU3 backend: " + Str(SDL_GetError()));
         }

         add_font_or_throw(io, resolve_runtime_asset_path("misc/fonts/SourceSerif/SourceSerif4-Regular.otf"));
         add_font_or_throw(io, resolve_runtime_asset_path("misc/fonts/SourceSans/SourceSans3-Regular.otf"));
         add_font_or_throw(io, resolve_runtime_asset_path("misc/fonts/SourceCodePro/SourceCodePro-Regular.otf"));

         store_imgui_state(context, state);
         return unit_cell();
       }},
      {"eventLoop",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         auto state = require_imgui_state("imgui.eventLoop", context);
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
         return unit_cell();
       }},
      {"checkMinimized",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         auto state = require_imgui_state("imgui.checkMinimized", context);
         if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MINIMIZED)
         {
           SDL_Delay(10);
           throw NextIteration{{}};
         }
         return unit_cell();
       }},
      {"NewFrame",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.NewFrame", context);
         ImGui_ImplSDLGPU3_NewFrame();
         ImGui_ImplSDL3_NewFrame();
         ImGui::NewFrame();
         return unit_cell();
        }},
      {"Begin",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
        {
           require_imgui_state("imgui.Begin", context);
           auto titleValue = require_string_arg("imgui.Begin", native_args_view(context, args), 0, "a string title");
           return make_runtime_boolean(ImGui::Begin(titleValue.c_str()));
         }},
      {"Text",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
        {
           require_imgui_state("imgui.Text", context);
           auto textValue = require_string_arg("imgui.Text", native_args_view(context, args), 0, "a string");
           ImGui::Text("%s", textValue.c_str());
           return unit_cell();
         }},
      {"End",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.End", context);
         ImGui::End();
         return unit_cell();
       }},
      {"Aborted",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
          auto state = require_imgui_state("imgui.Aborted", context);
         return make_runtime_boolean(state->done);
        }},
      {"Render",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         auto state = require_imgui_state("imgui.Render", context);
         ImGui::Render();

         ImDrawData *draw_data = ImGui::GetDrawData();
         const bool is_minimized = (draw_data->DisplaySize.x <= 0.0F || draw_data->DisplaySize.y <= 0.0F);

         SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(state->gpu_device);
         if (command_buffer == nullptr)
         {
           SDL_Log("imgui.Render(): SDL_AcquireGPUCommandBuffer() failed: %s", SDL_GetError());
           return unit_cell();
         }

         SDL_GPUTexture *swapchain_texture = nullptr;
         if (!SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer, state->window, &swapchain_texture, nullptr, nullptr))
         {
           SDL_Log("imgui.Render(): SDL_WaitAndAcquireGPUSwapchainTexture() failed: %s", SDL_GetError());
           SDL_CancelGPUCommandBuffer(command_buffer);
           return unit_cell();
         }
         if (swapchain_texture == nullptr)
         {
           SDL_CancelGPUCommandBuffer(command_buffer);
           return unit_cell();
         }

         if (swapchain_texture != nullptr && !is_minimized)
         {
           ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

           SDL_GPUColorTargetInfo target_info = {};
           target_info.texture = swapchain_texture;
           target_info.clear_color =
               SDL_FColor{state->clear_color.x, state->clear_color.y, state->clear_color.z, state->clear_color.w};
           target_info.load_op = SDL_GPU_LOADOP_CLEAR;
           target_info.store_op = SDL_GPU_STOREOP_STORE;
           target_info.mip_level = 0;
           target_info.layer_or_depth_plane = 0;
           target_info.cycle = false;
           SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

           ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

           SDL_EndGPURenderPass(render_pass);
         }

         if (!SDL_SubmitGPUCommandBuffer(command_buffer))
         {
           SDL_Log("imgui.Render(): SDL_SubmitGPUCommandBuffer() failed: %s", SDL_GetError());
         }
         return unit_cell();
       }},
      {"cleanup",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
          auto state = require_imgui_state("imgui.cleanup", context);
          state->shutdown();
         clear_imgui_state(context);
          return unit_cell();
        }},
      {"GetCurrentContext",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.GetCurrentContext", context);
         return borrowed_handle("ImGuiContext", ImGui::GetCurrentContext());
       }},
      {"SetCurrentContext",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetCurrentContext", context);
         auto nativeArgs = native_args_view(context, args);
         auto *ctx = require_handle_arg<ImGuiContext>("imgui.SetCurrentContext", nativeArgs, 0, "ImGuiContext");
         ImGui::SetCurrentContext(ctx);
         return unit_cell();
       }},
      {"GetIO",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.GetIO", context);
         return borrowed_handle("ImGuiIO", &ImGui::GetIO());
       }},
      {"GetStyle",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.GetStyle", context);
         return borrowed_handle("ImGuiStyle", &ImGui::GetStyle());
       }},
      {"ImGuiIO_GetConfigFlags",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetConfigFlags", native_args_view(context, args), 0,
                                               "ImGuiIO");
         return i32_cell(static_cast<int32_t>(io->ConfigFlags));
       }},
      {"ImGuiIO_SetConfigFlags",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_SetConfigFlags", nativeArgs, 0, "ImGuiIO");
         io->ConfigFlags = require_i32_arg("imgui.ImGuiIO_SetConfigFlags", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiIO_AddConfigFlags",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_AddConfigFlags", nativeArgs, 0, "ImGuiIO");
         io->ConfigFlags |= require_i32_arg("imgui.ImGuiIO_AddConfigFlags", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiIO_ClearConfigFlags",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_ClearConfigFlags", nativeArgs, 0, "ImGuiIO");
         io->ConfigFlags &= ~require_i32_arg("imgui.ImGuiIO_ClearConfigFlags", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiIO_GetFramerate",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetFramerate", native_args_view(context, args), 0,
                                               "ImGuiIO");
         return f32_cell(io->Framerate);
       }},
      {"ImGuiIO_GetDeltaTime",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetDeltaTime", native_args_view(context, args), 0,
                                               "ImGuiIO");
         return f32_cell(io->DeltaTime);
       }},
      {"ImGuiIO_GetWantCaptureMouse",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetWantCaptureMouse", native_args_view(context, args), 0,
                                               "ImGuiIO");
         return make_runtime_boolean(io->WantCaptureMouse);
       }},
      {"ImGuiIO_GetWantCaptureKeyboard",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetWantCaptureKeyboard", native_args_view(context, args),
                                               0, "ImGuiIO");
         return make_runtime_boolean(io->WantCaptureKeyboard);
       }},
      {"ImGuiIO_GetFontGlobalScale",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_GetFontGlobalScale", native_args_view(context, args), 0,
                                               "ImGuiIO");
         return f32_cell(io->FontGlobalScale);
       }},
      {"ImGuiIO_SetFontGlobalScale",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *io = require_handle_arg<ImGuiIO>("imgui.ImGuiIO_SetFontGlobalScale", nativeArgs, 0, "ImGuiIO");
         io->FontGlobalScale = require_f32_arg("imgui.ImGuiIO_SetFontGlobalScale", nativeArgs, 1);
         return unit_cell();
       }},
      {"StyleColorsDark",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.StyleColorsDark", context);
         ImGui::StyleColorsDark();
         return unit_cell();
       }},
      {"StyleColorsLight",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.StyleColorsLight", context);
         ImGui::StyleColorsLight();
         return unit_cell();
       }},
      {"StyleColorsClassic",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.StyleColorsClassic", context);
         ImGui::StyleColorsClassic();
         return unit_cell();
       }},
      {"ImGuiStyle_ScaleAllSizes",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *style = require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_ScaleAllSizes", nativeArgs, 0, "ImGuiStyle");
         style->ScaleAllSizes(require_f32_arg("imgui.ImGuiStyle_ScaleAllSizes", nativeArgs, 1));
         return unit_cell();
       }},
      {"ImGuiStyle_GetAlpha",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *style = require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_GetAlpha", native_args_view(context, args), 0,
                                                     "ImGuiStyle");
         return f32_cell(style->Alpha);
       }},
      {"ImGuiStyle_SetAlpha",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *style = require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_SetAlpha", nativeArgs, 0, "ImGuiStyle");
         style->Alpha = require_f32_arg("imgui.ImGuiStyle_SetAlpha", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiStyle_GetWindowRounding",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *style = require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_GetWindowRounding",
                                                     native_args_view(context, args), 0, "ImGuiStyle");
         return f32_cell(style->WindowRounding);
       }},
      {"ImGuiStyle_SetWindowRounding",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *style =
             require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_SetWindowRounding", nativeArgs, 0, "ImGuiStyle");
         style->WindowRounding = require_f32_arg("imgui.ImGuiStyle_SetWindowRounding", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiStyle_GetFrameRounding",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto *style = require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_GetFrameRounding",
                                                     native_args_view(context, args), 0, "ImGuiStyle");
         return f32_cell(style->FrameRounding);
       }},
      {"ImGuiStyle_SetFrameRounding",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         auto nativeArgs = native_args_view(context, args);
         auto *style =
             require_handle_arg<ImGuiStyle>("imgui.ImGuiStyle_SetFrameRounding", nativeArgs, 0, "ImGuiStyle");
         style->FrameRounding = require_f32_arg("imgui.ImGuiStyle_SetFrameRounding", nativeArgs, 1);
         return unit_cell();
       }},
      {"ImGuiConfigFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiConfigFlags_None); }},
      {"ImGuiConfigFlags_NavEnableKeyboard", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiConfigFlags_NavEnableKeyboard); }},
      {"ImGuiConfigFlags_NavEnableGamepad", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiConfigFlags_NavEnableGamepad); }},
      {"ImGuiConfigFlags_NoMouse", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiConfigFlags_NoMouse); }},
      {"ImGuiConfigFlags_NoKeyboard", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiConfigFlags_NoKeyboard); }},
      {"ImGuiWindowFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_None); }},
      {"ImGuiWindowFlags_NoTitleBar", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_NoTitleBar); }},
      {"ImGuiWindowFlags_NoResize", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_NoResize); }},
      {"ImGuiWindowFlags_NoMove", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_NoMove); }},
      {"ImGuiWindowFlags_NoScrollbar", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_NoScrollbar); }},
      {"ImGuiWindowFlags_NoCollapse", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_NoCollapse); }},
      {"ImGuiWindowFlags_AlwaysAutoResize", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_AlwaysAutoResize); }},
      {"ImGuiWindowFlags_MenuBar", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiWindowFlags_MenuBar); }},
      {"ImGuiCond_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCond_None); }},
      {"ImGuiCond_Always", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCond_Always); }},
      {"ImGuiCond_Once", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCond_Once); }},
      {"ImGuiCond_FirstUseEver", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCond_FirstUseEver); }},
      {"ImGuiCond_Appearing", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCond_Appearing); }},
      {"ImGuiMouseButton_Left", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiMouseButton_Left); }},
      {"ImGuiMouseButton_Right", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiMouseButton_Right); }},
      {"ImGuiMouseButton_Middle", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiMouseButton_Middle); }},
      {"ImGuiInputTextFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiInputTextFlags_None); }},
      {"ImGuiInputTextFlags_EnterReturnsTrue", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiInputTextFlags_EnterReturnsTrue); }},
      {"ImGuiInputTextFlags_ReadOnly", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiInputTextFlags_ReadOnly); }},
      {"ImGuiInputTextFlags_Password", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiInputTextFlags_Password); }},
      {"ImGuiInputTextFlags_AutoSelectAll", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiInputTextFlags_AutoSelectAll); }},
      {"ImGuiSelectableFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiSelectableFlags_None); }},
      {"ImGuiSelectableFlags_SpanAllColumns", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiSelectableFlags_SpanAllColumns); }},
      {"ImGuiSelectableFlags_AllowDoubleClick", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiSelectableFlags_AllowDoubleClick); }},
      {"ImGuiSelectableFlags_Disabled", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiSelectableFlags_Disabled); }},
      {"ImGuiComboFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiComboFlags_None); }},
      {"ImGuiComboFlags_HeightSmall", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiComboFlags_HeightSmall); }},
      {"ImGuiComboFlags_HeightRegular", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiComboFlags_HeightRegular); }},
      {"ImGuiComboFlags_NoArrowButton", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiComboFlags_NoArrowButton); }},
      {"ImGuiTableFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_None); }},
      {"ImGuiTableFlags_Resizable", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_Resizable); }},
      {"ImGuiTableFlags_Reorderable", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_Reorderable); }},
      {"ImGuiTableFlags_Hideable", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_Hideable); }},
      {"ImGuiTableFlags_Sortable", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_Sortable); }},
      {"ImGuiTableFlags_RowBg", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_RowBg); }},
      {"ImGuiTableFlags_Borders", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_Borders); }},
      {"ImGuiTableFlags_ScrollY", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableFlags_ScrollY); }},
      {"ImGuiTableColumnFlags_None", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableColumnFlags_None); }},
      {"ImGuiTableColumnFlags_WidthStretch", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableColumnFlags_WidthStretch); }},
      {"ImGuiTableColumnFlags_WidthFixed", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiTableColumnFlags_WidthFixed); }},
      {"ImGuiCol_Text", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_Text); }},
      {"ImGuiCol_WindowBg", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_WindowBg); }},
      {"ImGuiCol_FrameBg", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_FrameBg); }},
      {"ImGuiCol_Button", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_Button); }},
      {"ImGuiCol_ButtonHovered", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_ButtonHovered); }},
      {"ImGuiCol_Header", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiCol_Header); }},
      {"ImGuiStyleVar_Alpha", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiStyleVar_Alpha); }},
      {"ImGuiStyleVar_WindowRounding", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiStyleVar_WindowRounding); }},
      {"ImGuiStyleVar_FrameRounding", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiStyleVar_FrameRounding); }},
      {"ImGuiStyleVar_FramePadding", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiStyleVar_FramePadding); }},
      {"ImGuiStyleVar_ItemSpacing", [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> { return i32_cell(ImGuiStyleVar_ItemSpacing); }},
      {"BeginFlags",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginFlags", context);
         auto nativeArgs = native_args_view(context, args);
         auto title = require_string_arg("imgui.BeginFlags", nativeArgs, 0, "a string title");
         auto flags = require_i32_arg("imgui.BeginFlags", nativeArgs, 1);
         return make_runtime_boolean(ImGui::Begin(title.c_str(), nullptr, flags));
       }},
      {"BeginChild",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginChild", context);
         auto nativeArgs = native_args_view(context, args);
         auto id = require_string_arg("imgui.BeginChild", nativeArgs, 0, "a string id");
         auto width = require_f32_arg("imgui.BeginChild", nativeArgs, 1);
         auto height = require_f32_arg("imgui.BeginChild", nativeArgs, 2);
         auto border = require_bool_arg("imgui.BeginChild", nativeArgs, 3);
         auto flags = require_i32_arg("imgui.BeginChild", nativeArgs, 4);
         auto childFlags = border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
         return make_runtime_boolean(ImGui::BeginChild(id.c_str(), ImVec2(width, height), childFlags, flags));
       }},
      {"EndChild",
       [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.EndChild", context);
         ImGui::EndChild();
         return unit_cell();
       }},
      {"SetNextWindowSize",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetNextWindowSize", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::SetNextWindowSize(ImVec2(require_f32_arg("imgui.SetNextWindowSize", nativeArgs, 0),
                                         require_f32_arg("imgui.SetNextWindowSize", nativeArgs, 1)),
                                  require_i32_arg("imgui.SetNextWindowSize", nativeArgs, 2));
         return unit_cell();
       }},
      {"SetNextWindowPos",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetNextWindowPos", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::SetNextWindowPos(ImVec2(require_f32_arg("imgui.SetNextWindowPos", nativeArgs, 0),
                                        require_f32_arg("imgui.SetNextWindowPos", nativeArgs, 1)),
                                 require_i32_arg("imgui.SetNextWindowPos", nativeArgs, 2));
         return unit_cell();
       }},
      {"SetNextWindowCollapsed",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetNextWindowCollapsed", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::SetNextWindowCollapsed(require_bool_arg("imgui.SetNextWindowCollapsed", nativeArgs, 0),
                                       require_i32_arg("imgui.SetNextWindowCollapsed", nativeArgs, 1));
         return unit_cell();
       }},
      {"SetNextWindowFocus", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.SetNextWindowFocus", context); ImGui::SetNextWindowFocus(); return unit_cell(); }},
      {"SetNextWindowBgAlpha",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetNextWindowBgAlpha", context);
         ImGui::SetNextWindowBgAlpha(require_f32_arg("imgui.SetNextWindowBgAlpha", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetWindowCollapsed",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetWindowCollapsed", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::SetWindowCollapsed(require_bool_arg("imgui.SetWindowCollapsed", nativeArgs, 0),
                                   require_i32_arg("imgui.SetWindowCollapsed", nativeArgs, 1));
         return unit_cell();
       }},
      {"SetWindowFocus", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.SetWindowFocus", context); ImGui::SetWindowFocus(); return unit_cell(); }},
      {"SetWindowFontScale",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetWindowFontScale", context);
         ImGui::SetWindowFontScale(require_f32_arg("imgui.SetWindowFontScale", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"GetWindowPosX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowPosX", context); return f32_cell(ImGui::GetWindowPos().x); }},
      {"GetWindowPosY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowPosY", context); return f32_cell(ImGui::GetWindowPos().y); }},
      {"GetWindowSizeX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowSizeX", context); return f32_cell(ImGui::GetWindowSize().x); }},
      {"GetWindowSizeY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowSizeY", context); return f32_cell(ImGui::GetWindowSize().y); }},
      {"GetWindowWidth", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowWidth", context); return f32_cell(ImGui::GetWindowWidth()); }},
      {"GetWindowHeight", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetWindowHeight", context); return f32_cell(ImGui::GetWindowHeight()); }},
      {"Separator", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.Separator", context); ImGui::Separator(); return unit_cell(); }},
      {"SeparatorText",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SeparatorText", context);
         auto label = require_string_arg("imgui.SeparatorText", native_args_view(context, args), 0, "a label");
         ImGui::SeparatorText(label.c_str());
         return unit_cell();
       }},
      {"SameLine", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.SameLine", context); ImGui::SameLine(); return unit_cell(); }},
      {"NewLine", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.NewLine", context); ImGui::NewLine(); return unit_cell(); }},
      {"Spacing", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.Spacing", context); ImGui::Spacing(); return unit_cell(); }},
      {"Dummy",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Dummy", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::Dummy(ImVec2(require_f32_arg("imgui.Dummy", nativeArgs, 0),
                             require_f32_arg("imgui.Dummy", nativeArgs, 1)));
         return unit_cell();
       }},
      {"Indent",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Indent", context);
         ImGui::Indent(require_f32_arg("imgui.Indent", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"Unindent",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Unindent", context);
         ImGui::Unindent(require_f32_arg("imgui.Unindent", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"PushID",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PushID", context);
         auto id = require_string_arg("imgui.PushID", native_args_view(context, args), 0, "an id");
         ImGui::PushID(id.c_str());
         return unit_cell();
       }},
      {"PushIDInt",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PushIDInt", context);
         ImGui::PushID(require_i32_arg("imgui.PushIDInt", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"PopID", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.PopID", context); ImGui::PopID(); return unit_cell(); }},
      {"BeginGroup", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.BeginGroup", context); ImGui::BeginGroup(); return unit_cell(); }},
      {"EndGroup", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndGroup", context); ImGui::EndGroup(); return unit_cell(); }},
      {"GetContentRegionAvailX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetContentRegionAvailX", context); return f32_cell(ImGui::GetContentRegionAvail().x); }},
      {"GetContentRegionAvailY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetContentRegionAvailY", context); return f32_cell(ImGui::GetContentRegionAvail().y); }},
      {"GetCursorPosX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetCursorPosX", context); return f32_cell(ImGui::GetCursorPosX()); }},
      {"GetCursorPosY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetCursorPosY", context); return f32_cell(ImGui::GetCursorPosY()); }},
      {"GetTextLineHeight", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetTextLineHeight", context); return f32_cell(ImGui::GetTextLineHeight()); }},
      {"GetTextLineHeightWithSpacing", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetTextLineHeightWithSpacing", context); return f32_cell(ImGui::GetTextLineHeightWithSpacing()); }},
      {"GetFrameHeight", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetFrameHeight", context); return f32_cell(ImGui::GetFrameHeight()); }},
      {"GetFrameHeightWithSpacing", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetFrameHeightWithSpacing", context); return f32_cell(ImGui::GetFrameHeightWithSpacing()); }},
      {"SetCursorPos",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetCursorPos", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::SetCursorPos(ImVec2(require_f32_arg("imgui.SetCursorPos", nativeArgs, 0),
                                    require_f32_arg("imgui.SetCursorPos", nativeArgs, 1)));
         return unit_cell();
       }},
      {"SetCursorPosX",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetCursorPosX", context);
         ImGui::SetCursorPosX(require_f32_arg("imgui.SetCursorPosX", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetCursorPosY",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetCursorPosY", context);
         ImGui::SetCursorPosY(require_f32_arg("imgui.SetCursorPosY", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"AlignTextToFramePadding", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.AlignTextToFramePadding", context); ImGui::AlignTextToFramePadding(); return unit_cell(); }},
      {"GetScrollX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetScrollX", context); return f32_cell(ImGui::GetScrollX()); }},
      {"GetScrollY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetScrollY", context); return f32_cell(ImGui::GetScrollY()); }},
      {"SetScrollX",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetScrollX", context);
         ImGui::SetScrollX(require_f32_arg("imgui.SetScrollX", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetScrollY",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetScrollY", context);
         ImGui::SetScrollY(require_f32_arg("imgui.SetScrollY", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"GetScrollMaxX", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetScrollMaxX", context); return f32_cell(ImGui::GetScrollMaxX()); }},
      {"GetScrollMaxY", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetScrollMaxY", context); return f32_cell(ImGui::GetScrollMaxY()); }},
      {"SetScrollHereX",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetScrollHereX", context);
         ImGui::SetScrollHereX(require_f32_arg("imgui.SetScrollHereX", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetScrollHereY",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetScrollHereY", context);
         ImGui::SetScrollHereY(require_f32_arg("imgui.SetScrollHereY", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetItemDefaultFocus", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.SetItemDefaultFocus", context); ImGui::SetItemDefaultFocus(); return unit_cell(); }},
      {"SetKeyboardFocusHere",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetKeyboardFocusHere", context);
         ImGui::SetKeyboardFocusHere(require_i32_arg("imgui.SetKeyboardFocusHere", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"SetNextItemAllowOverlap", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.SetNextItemAllowOverlap", context); ImGui::SetNextItemAllowOverlap(); return unit_cell(); }},
      {"BeginDisabled",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginDisabled", context);
         ImGui::BeginDisabled(require_bool_arg("imgui.BeginDisabled", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"EndDisabled", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndDisabled", context); ImGui::EndDisabled(); return unit_cell(); }},
      {"BeginTooltip", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.BeginTooltip", context); return make_runtime_boolean(ImGui::BeginTooltip()); }},
      {"EndTooltip", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndTooltip", context); ImGui::EndTooltip(); return unit_cell(); }},
      {"SetTooltip",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetTooltip", context);
         auto text = require_string_arg("imgui.SetTooltip", native_args_view(context, args), 0, "tooltip text");
         ImGui::SetTooltip("%s", text.c_str());
         return unit_cell();
       }},
      {"SetItemTooltip",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SetItemTooltip", context);
         auto text = require_string_arg("imgui.SetItemTooltip", native_args_view(context, args), 0, "tooltip text");
         ImGui::SetItemTooltip("%s", text.c_str());
         return unit_cell();
       }},
      {"PushStyleColor",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PushStyleColor", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::PushStyleColor(require_i32_arg("imgui.PushStyleColor", nativeArgs, 0),
                               ImVec4(require_f32_arg("imgui.PushStyleColor", nativeArgs, 1),
                                      require_f32_arg("imgui.PushStyleColor", nativeArgs, 2),
                                      require_f32_arg("imgui.PushStyleColor", nativeArgs, 3),
                                      require_f32_arg("imgui.PushStyleColor", nativeArgs, 4)));
         return unit_cell();
       }},
      {"PopStyleColor",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PopStyleColor", context);
         ImGui::PopStyleColor(require_i32_arg("imgui.PopStyleColor", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"PushStyleVarFloat",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PushStyleVarFloat", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::PushStyleVar(require_i32_arg("imgui.PushStyleVarFloat", nativeArgs, 0),
                             require_f32_arg("imgui.PushStyleVarFloat", nativeArgs, 1));
         return unit_cell();
       }},
      {"PushStyleVarVec2",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PushStyleVarVec2", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::PushStyleVar(require_i32_arg("imgui.PushStyleVarVec2", nativeArgs, 0),
                             ImVec2(require_f32_arg("imgui.PushStyleVarVec2", nativeArgs, 1),
                                    require_f32_arg("imgui.PushStyleVarVec2", nativeArgs, 2)));
         return unit_cell();
       }},
      {"PopStyleVar",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.PopStyleVar", context);
         ImGui::PopStyleVar(require_i32_arg("imgui.PopStyleVar", native_args_view(context, args), 0));
         return unit_cell();
       }},
      {"TextDisabled",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TextDisabled", context);
         auto text = require_string_arg("imgui.TextDisabled", native_args_view(context, args), 0, "a string");
         ImGui::TextDisabled("%s", text.c_str());
         return unit_cell();
       }},
      {"TextWrapped",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TextWrapped", context);
         auto text = require_string_arg("imgui.TextWrapped", native_args_view(context, args), 0, "a string");
         ImGui::TextWrapped("%s", text.c_str());
         return unit_cell();
       }},
      {"TextNgHighlighted",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TextNgHighlighted", context);
         auto source = require_string_arg("imgui.TextNgHighlighted", native_args_view(context, args), 0, "source");
         render_highlighted_ng_source(source);
         return unit_cell();
       }},
      {"LabelText",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.LabelText", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.LabelText", nativeArgs, 0, "a label");
         auto text = require_string_arg("imgui.LabelText", nativeArgs, 1, "text");
         ImGui::LabelText(label.c_str(), "%s", text.c_str());
         return unit_cell();
       }},
      {"BulletText",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BulletText", context);
         auto text = require_string_arg("imgui.BulletText", native_args_view(context, args), 0, "a string");
         ImGui::BulletText("%s", text.c_str());
         return unit_cell();
       }},
      {"TextColored",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TextColored", context);
         auto nativeArgs = native_args_view(context, args);
         auto text = require_string_arg("imgui.TextColored", nativeArgs, 4, "text");
         ImGui::TextColored(ImVec4(require_f32_arg("imgui.TextColored", nativeArgs, 0),
                                   require_f32_arg("imgui.TextColored", nativeArgs, 1),
                                   require_f32_arg("imgui.TextColored", nativeArgs, 2),
                                   require_f32_arg("imgui.TextColored", nativeArgs, 3)),
                            "%s", text.c_str());
         return unit_cell();
       }},
      {"Bullet", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.Bullet", context); ImGui::Bullet(); return unit_cell(); }},
      {"Button",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Button", context);
         auto label = require_string_arg("imgui.Button", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::Button(label.c_str()));
       }},
      {"ButtonSized",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.ButtonSized", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.ButtonSized", nativeArgs, 0, "a label");
         return make_runtime_boolean(ImGui::Button(label.c_str(),
                                                   ImVec2(require_f32_arg("imgui.ButtonSized", nativeArgs, 1),
                                                          require_f32_arg("imgui.ButtonSized", nativeArgs, 2))));
       }},
      {"SmallButton",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SmallButton", context);
         auto label = require_string_arg("imgui.SmallButton", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::SmallButton(label.c_str()));
       }},
      {"Checkbox",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Checkbox", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.Checkbox", nativeArgs, 0, "a label");
         bool value = require_bool_arg("imgui.Checkbox", nativeArgs, 1);
         ImGui::Checkbox(label.c_str(), &value);
         return make_runtime_boolean(value);
       }},
      {"RadioButton",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.RadioButton", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.RadioButton", nativeArgs, 0, "a label");
         return make_runtime_boolean(ImGui::RadioButton(label.c_str(), require_bool_arg("imgui.RadioButton", nativeArgs, 1)));
       }},
      {"SliderFloat",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SliderFloat", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.SliderFloat", nativeArgs, 0, "a label");
         float value = require_f32_arg("imgui.SliderFloat", nativeArgs, 1);
         ImGui::SliderFloat(label.c_str(), &value, require_f32_arg("imgui.SliderFloat", nativeArgs, 2),
                            require_f32_arg("imgui.SliderFloat", nativeArgs, 3));
         return f32_cell(value);
       }},
      {"SliderInt",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.SliderInt", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.SliderInt", nativeArgs, 0, "a label");
         int value = require_i32_arg("imgui.SliderInt", nativeArgs, 1);
         ImGui::SliderInt(label.c_str(), &value, require_i32_arg("imgui.SliderInt", nativeArgs, 2),
                          require_i32_arg("imgui.SliderInt", nativeArgs, 3));
         return i32_cell(value);
       }},
      {"DragFloat",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.DragFloat", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.DragFloat", nativeArgs, 0, "a label");
         float value = require_f32_arg("imgui.DragFloat", nativeArgs, 1);
         ImGui::DragFloat(label.c_str(), &value, require_f32_arg("imgui.DragFloat", nativeArgs, 2),
                          require_f32_arg("imgui.DragFloat", nativeArgs, 3),
                          require_f32_arg("imgui.DragFloat", nativeArgs, 4));
         return f32_cell(value);
       }},
      {"DragInt",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.DragInt", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.DragInt", nativeArgs, 0, "a label");
         int value = require_i32_arg("imgui.DragInt", nativeArgs, 1);
         ImGui::DragInt(label.c_str(), &value, require_f32_arg("imgui.DragInt", nativeArgs, 2),
                        require_i32_arg("imgui.DragInt", nativeArgs, 3),
                        require_i32_arg("imgui.DragInt", nativeArgs, 4));
         return i32_cell(value);
       }},
      {"InputText",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.InputText", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.InputText", nativeArgs, 0, "a label");
         auto value = require_string_arg("imgui.InputText", nativeArgs, 1, "text");
         auto buffer = input_text_buffer(value);
         ImGui::InputText(label.c_str(), buffer.data(), buffer.size(),
                          require_i32_arg("imgui.InputText", nativeArgs, 2));
         return make_runtime_string(buffer.c_str());
       }},
      {"InputTextMultiline",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.InputTextMultiline", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.InputTextMultiline", nativeArgs, 0, "a label");
         auto value = require_string_arg("imgui.InputTextMultiline", nativeArgs, 1, "text");
         auto buffer = multiline_text_buffer(value);
         ImGui::InputTextMultiline(label.c_str(), buffer.data(), buffer.size(),
                                   ImVec2(require_f32_arg("imgui.InputTextMultiline", nativeArgs, 2),
                                          require_f32_arg("imgui.InputTextMultiline", nativeArgs, 3)),
                                   require_i32_arg("imgui.InputTextMultiline", nativeArgs, 4));
         return make_runtime_string(buffer.c_str());
       }},
      {"InputFloat",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.InputFloat", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.InputFloat", nativeArgs, 0, "a label");
         float value = require_f32_arg("imgui.InputFloat", nativeArgs, 1);
         ImGui::InputFloat(label.c_str(), &value, require_f32_arg("imgui.InputFloat", nativeArgs, 2),
                           require_f32_arg("imgui.InputFloat", nativeArgs, 3), "%.3f",
                           require_i32_arg("imgui.InputFloat", nativeArgs, 4));
         return f32_cell(value);
       }},
      {"InputInt",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.InputInt", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.InputInt", nativeArgs, 0, "a label");
         int value = require_i32_arg("imgui.InputInt", nativeArgs, 1);
         ImGui::InputInt(label.c_str(), &value, require_i32_arg("imgui.InputInt", nativeArgs, 2),
                         require_i32_arg("imgui.InputInt", nativeArgs, 3),
                         require_i32_arg("imgui.InputInt", nativeArgs, 4));
         return i32_cell(value);
       }},
      {"Selectable",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.Selectable", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.Selectable", nativeArgs, 0, "a label");
         return make_runtime_boolean(ImGui::Selectable(label.c_str(),
                                                       require_bool_arg("imgui.Selectable", nativeArgs, 1),
                                                       require_i32_arg("imgui.Selectable", nativeArgs, 2),
                                                       ImVec2(require_f32_arg("imgui.Selectable", nativeArgs, 3),
                                                              require_f32_arg("imgui.Selectable", nativeArgs, 4))));
       }},
      {"BeginCombo",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginCombo", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.BeginCombo", nativeArgs, 0, "a label");
         auto preview = require_string_arg("imgui.BeginCombo", nativeArgs, 1, "preview text");
         return make_runtime_boolean(
             ImGui::BeginCombo(label.c_str(), preview.c_str(), require_i32_arg("imgui.BeginCombo", nativeArgs, 2)));
       }},
      {"EndCombo", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndCombo", context); ImGui::EndCombo(); return unit_cell(); }},
      {"ProgressBar",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.ProgressBar", context);
         auto nativeArgs = native_args_view(context, args);
         auto overlay = require_string_arg("imgui.ProgressBar", nativeArgs, 3, "overlay text");
         ImGui::ProgressBar(require_f32_arg("imgui.ProgressBar", nativeArgs, 0),
                            ImVec2(require_f32_arg("imgui.ProgressBar", nativeArgs, 1),
                                   require_f32_arg("imgui.ProgressBar", nativeArgs, 2)),
                            overlay.empty() ? nullptr : overlay.c_str());
         return unit_cell();
       }},
      {"BeginTable",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginTable", context);
         auto nativeArgs = native_args_view(context, args);
         auto id = require_string_arg("imgui.BeginTable", nativeArgs, 0, "an id");
         return make_runtime_boolean(ImGui::BeginTable(id.c_str(), require_i32_arg("imgui.BeginTable", nativeArgs, 1),
                                                       require_i32_arg("imgui.BeginTable", nativeArgs, 2),
                                                       ImVec2(require_f32_arg("imgui.BeginTable", nativeArgs, 3),
                                                              require_f32_arg("imgui.BeginTable", nativeArgs, 4)),
                                                       require_f32_arg("imgui.BeginTable", nativeArgs, 5)));
       }},
      {"EndTable", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndTable", context); ImGui::EndTable(); return unit_cell(); }},
      {"TableNextRow",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TableNextRow", context);
         auto nativeArgs = native_args_view(context, args);
         ImGui::TableNextRow(require_i32_arg("imgui.TableNextRow", nativeArgs, 0),
                             require_f32_arg("imgui.TableNextRow", nativeArgs, 1));
         return unit_cell();
       }},
      {"TableNextColumn", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TableNextColumn", context); return make_runtime_boolean(ImGui::TableNextColumn()); }},
      {"TableSetColumnIndex",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TableSetColumnIndex", context);
         return make_runtime_boolean(
             ImGui::TableSetColumnIndex(require_i32_arg("imgui.TableSetColumnIndex", native_args_view(context, args), 0)));
       }},
      {"TableSetupColumn",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TableSetupColumn", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.TableSetupColumn", nativeArgs, 0, "a label");
         ImGui::TableSetupColumn(label.c_str(), require_i32_arg("imgui.TableSetupColumn", nativeArgs, 1),
                                 require_f32_arg("imgui.TableSetupColumn", nativeArgs, 2));
         return unit_cell();
       }},
      {"TableHeadersRow", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TableHeadersRow", context); ImGui::TableHeadersRow(); return unit_cell(); }},
      {"TableHeader",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TableHeader", context);
         auto label = require_string_arg("imgui.TableHeader", native_args_view(context, args), 0, "a label");
         ImGui::TableHeader(label.c_str());
         return unit_cell();
       }},
      {"TableGetColumnCount", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TableGetColumnCount", context); return i32_cell(ImGui::TableGetColumnCount()); }},
      {"TableGetColumnIndex", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TableGetColumnIndex", context); return i32_cell(ImGui::TableGetColumnIndex()); }},
      {"TableGetRowIndex", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TableGetRowIndex", context); return i32_cell(ImGui::TableGetRowIndex()); }},
      {"CollapsingHeader",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.CollapsingHeader", context);
         auto label = require_string_arg("imgui.CollapsingHeader", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::CollapsingHeader(label.c_str()));
       }},
      {"TreeNode",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.TreeNode", context);
         auto label = require_string_arg("imgui.TreeNode", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::TreeNode(label.c_str()));
       }},
      {"TreePop", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.TreePop", context); ImGui::TreePop(); return unit_cell(); }},
      {"BeginTabBar",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginTabBar", context);
         auto id = require_string_arg("imgui.BeginTabBar", native_args_view(context, args), 0, "an id");
         return make_runtime_boolean(ImGui::BeginTabBar(id.c_str()));
       }},
      {"EndTabBar", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndTabBar", context); ImGui::EndTabBar(); return unit_cell(); }},
      {"BeginTabItem",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginTabItem", context);
         auto label = require_string_arg("imgui.BeginTabItem", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::BeginTabItem(label.c_str()));
       }},
      {"EndTabItem", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndTabItem", context); ImGui::EndTabItem(); return unit_cell(); }},
      {"BeginMenuBar", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.BeginMenuBar", context); return make_runtime_boolean(ImGui::BeginMenuBar()); }},
      {"EndMenuBar", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndMenuBar", context); ImGui::EndMenuBar(); return unit_cell(); }},
      {"BeginMenu",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginMenu", context);
         auto label = require_string_arg("imgui.BeginMenu", native_args_view(context, args), 0, "a label");
         return make_runtime_boolean(ImGui::BeginMenu(label.c_str()));
       }},
      {"EndMenu", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndMenu", context); ImGui::EndMenu(); return unit_cell(); }},
      {"MenuItem",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.MenuItem", context);
         auto nativeArgs = native_args_view(context, args);
         auto label = require_string_arg("imgui.MenuItem", nativeArgs, 0, "a label");
         return make_runtime_boolean(ImGui::MenuItem(label.c_str(), nullptr, require_bool_arg("imgui.MenuItem", nativeArgs, 1)));
       }},
      {"OpenPopup",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.OpenPopup", context);
         auto id = require_string_arg("imgui.OpenPopup", native_args_view(context, args), 0, "an id");
         ImGui::OpenPopup(id.c_str());
         return unit_cell();
       }},
      {"BeginPopup",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.BeginPopup", context);
         auto id = require_string_arg("imgui.BeginPopup", native_args_view(context, args), 0, "an id");
         return make_runtime_boolean(ImGui::BeginPopup(id.c_str()));
       }},
      {"EndPopup", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.EndPopup", context); ImGui::EndPopup(); return unit_cell(); }},
      {"IsItemHovered", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemHovered", context); return make_runtime_boolean(ImGui::IsItemHovered()); }},
      {"IsItemClicked",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.IsItemClicked", context);
         return make_runtime_boolean(ImGui::IsItemClicked(require_i32_arg("imgui.IsItemClicked", native_args_view(context, args), 0)));
       }},
      {"IsItemActive", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemActive", context); return make_runtime_boolean(ImGui::IsItemActive()); }},
      {"IsItemFocused", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemFocused", context); return make_runtime_boolean(ImGui::IsItemFocused()); }},
      {"IsItemEdited", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemEdited", context); return make_runtime_boolean(ImGui::IsItemEdited()); }},
      {"IsItemVisible", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemVisible", context); return make_runtime_boolean(ImGui::IsItemVisible()); }},
      {"IsItemDeactivatedAfterEdit", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsItemDeactivatedAfterEdit", context); return make_runtime_boolean(ImGui::IsItemDeactivatedAfterEdit()); }},
      {"IsAnyItemHovered", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsAnyItemHovered", context); return make_runtime_boolean(ImGui::IsAnyItemHovered()); }},
      {"IsAnyItemActive", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsAnyItemActive", context); return make_runtime_boolean(ImGui::IsAnyItemActive()); }},
      {"IsAnyItemFocused", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsAnyItemFocused", context); return make_runtime_boolean(ImGui::IsAnyItemFocused()); }},
      {"IsWindowHovered", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsWindowHovered", context); return make_runtime_boolean(ImGui::IsWindowHovered()); }},
      {"IsWindowFocused", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsWindowFocused", context); return make_runtime_boolean(ImGui::IsWindowFocused()); }},
      {"IsWindowAppearing", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsWindowAppearing", context); return make_runtime_boolean(ImGui::IsWindowAppearing()); }},
      {"IsWindowCollapsed", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.IsWindowCollapsed", context); return make_runtime_boolean(ImGui::IsWindowCollapsed()); }},
      {"IsMouseClicked",
       [](const NGSelf &, const NGEnv &context, const NGArgs &args) -> RuntimeRef<StorageCell>
       {
         require_imgui_state("imgui.IsMouseClicked", context);
         return make_runtime_boolean(ImGui::IsMouseClicked(require_i32_arg("imgui.IsMouseClicked", native_args_view(context, args), 0)));
       }},
      {"GetTime", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetTime", context); return f64_cell(ImGui::GetTime()); }},
      {"GetFrameCount", [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> { require_imgui_state("imgui.GetFrameCount", context); return i32_cell(ImGui::GetFrameCount()); }},
  };

  void do_register()
  {
    register_native_library("std.imgui", handlers);
    using namespace NG::typecheck;
    auto unitType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
    auto boolType = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
    auto stringType = makecheck<PrimitiveType>(typeinfo_tag::STRING);
    auto i32Type = makecheck<PrimitiveType>(typeinfo_tag::I32);
    auto f32Type = makecheck<PrimitiveType>(typeinfo_tag::F32);
    auto f64Type = makecheck<PrimitiveType>(typeinfo_tag::F64);
    auto contextType = makecheck<CustomizedType>("ImGuiContext", true, false, "std.imgui");
    auto ioType = makecheck<CustomizedType>("ImGuiIO", true, false, "std.imgui");
    auto styleType = makecheck<CustomizedType>("ImGuiStyle", true, false, "std.imgui");

    auto descriptor = makert<NG::module::NativeModuleDescriptor>();
    descriptor->moduleId = "std.imgui";
    descriptor->functions = handlers;
    descriptor->exports.declared.insert("*");

    descriptor->typeIndex.insert_or_assign("ImGuiContext", contextType);
    descriptor->typeIndex.insert_or_assign("ImGuiIO", ioType);
    descriptor->typeIndex.insert_or_assign("ImGuiStyle", styleType);

    auto addFunction = [&](const Str &name, CheckingRef<TypeInfo> ret, Vec<CheckingRef<TypeInfo>> params = {}) {
      descriptor->typeIndex.insert_or_assign(name, makecheck<FunctionType>(std::move(ret), std::move(params)));
    };
    auto addUnit0 = [&](const Str &name) { addFunction(name, unitType); };
    auto addBool0 = [&](const Str &name) { addFunction(name, boolType); };
    auto addI320 = [&](const Str &name) { addFunction(name, i32Type); };
    auto addF320 = [&](const Str &name) { addFunction(name, f32Type); };

    for (const auto &name : Vec<Str>{"init", "eventLoop", "checkMinimized", "NewFrame", "Render", "cleanup",
                                     "End", "EndChild", "StyleColorsDark", "StyleColorsLight",
                                     "StyleColorsClassic", "Separator", "SameLine", "NewLine", "Spacing",
                                     "SetNextWindowFocus", "SetWindowFocus", "PopID", "BeginGroup", "EndGroup",
                                     "AlignTextToFramePadding", "SetItemDefaultFocus", "SetNextItemAllowOverlap",
                                     "EndDisabled", "EndTooltip", "Bullet", "EndCombo", "EndTable",
                                     "TableHeadersRow", "TreePop", "EndTabBar", "EndTabItem", "EndMenuBar",
                                     "EndMenu", "EndPopup"})
    {
      addUnit0(name);
    }
    for (const auto &name : Vec<Str>{"Aborted", "BeginMenuBar", "BeginTooltip", "TableNextColumn",
                                     "IsItemHovered", "IsItemActive", "IsItemFocused", "IsItemEdited",
                                     "IsItemVisible", "IsItemDeactivatedAfterEdit", "IsAnyItemHovered",
                                     "IsAnyItemActive", "IsAnyItemFocused", "IsWindowHovered", "IsWindowFocused",
                                     "IsWindowAppearing", "IsWindowCollapsed"})
    {
      addBool0(name);
    }
    for (const auto &name : Vec<Str>{"ImGuiConfigFlags_None", "ImGuiConfigFlags_NavEnableKeyboard",
                                     "ImGuiConfigFlags_NavEnableGamepad", "ImGuiConfigFlags_NoMouse",
                                     "ImGuiConfigFlags_NoKeyboard", "ImGuiWindowFlags_None",
                                     "ImGuiWindowFlags_NoTitleBar", "ImGuiWindowFlags_NoResize",
                                     "ImGuiWindowFlags_NoMove", "ImGuiWindowFlags_NoScrollbar",
                                     "ImGuiWindowFlags_NoCollapse", "ImGuiWindowFlags_AlwaysAutoResize",
                                     "ImGuiWindowFlags_MenuBar", "ImGuiCond_None", "ImGuiCond_Always",
                                     "ImGuiCond_Once", "ImGuiCond_FirstUseEver", "ImGuiCond_Appearing",
                                     "ImGuiMouseButton_Left", "ImGuiMouseButton_Right", "ImGuiMouseButton_Middle",
                                     "ImGuiInputTextFlags_None", "ImGuiInputTextFlags_EnterReturnsTrue",
                                     "ImGuiInputTextFlags_ReadOnly", "ImGuiInputTextFlags_Password",
                                     "ImGuiInputTextFlags_AutoSelectAll", "ImGuiSelectableFlags_None",
                                     "ImGuiSelectableFlags_SpanAllColumns", "ImGuiSelectableFlags_AllowDoubleClick",
                                     "ImGuiSelectableFlags_Disabled", "ImGuiComboFlags_None",
                                     "ImGuiComboFlags_HeightSmall", "ImGuiComboFlags_HeightRegular",
                                     "ImGuiComboFlags_NoArrowButton", "ImGuiTableFlags_None",
                                     "ImGuiTableFlags_Resizable", "ImGuiTableFlags_Reorderable",
                                     "ImGuiTableFlags_Hideable", "ImGuiTableFlags_Sortable",
                                     "ImGuiTableFlags_RowBg", "ImGuiTableFlags_Borders", "ImGuiTableFlags_ScrollY",
                                     "ImGuiTableColumnFlags_None", "ImGuiTableColumnFlags_WidthStretch",
                                     "ImGuiTableColumnFlags_WidthFixed", "ImGuiCol_Text", "ImGuiCol_WindowBg",
                                     "ImGuiCol_FrameBg", "ImGuiCol_Button", "ImGuiCol_ButtonHovered",
                                     "ImGuiCol_Header", "ImGuiStyleVar_Alpha", "ImGuiStyleVar_WindowRounding",
                                     "ImGuiStyleVar_FrameRounding", "ImGuiStyleVar_FramePadding",
                                     "ImGuiStyleVar_ItemSpacing", "TableGetColumnCount", "TableGetColumnIndex",
                                     "TableGetRowIndex", "GetFrameCount"})
    {
      addI320(name);
    }
    for (const auto &name : Vec<Str>{"GetWindowPosX", "GetWindowPosY", "GetWindowSizeX", "GetWindowSizeY",
                                     "GetWindowWidth", "GetWindowHeight", "GetContentRegionAvailX",
                                     "GetContentRegionAvailY", "GetCursorPosX", "GetCursorPosY",
                                     "GetTextLineHeight", "GetTextLineHeightWithSpacing", "GetFrameHeight",
                                     "GetFrameHeightWithSpacing", "GetScrollX", "GetScrollY", "GetScrollMaxX",
                                     "GetScrollMaxY"})
    {
      addF320(name);
    }
    addFunction("GetVersion", stringType);
    addFunction("GetCurrentContext", contextType);
    addFunction("SetCurrentContext", unitType, {contextType});
    addFunction("GetIO", ioType);
    addFunction("GetStyle", styleType);
    addFunction("GetTime", f64Type);

    addFunction("ImGuiIO_GetConfigFlags", i32Type, {ioType});
    addFunction("ImGuiIO_SetConfigFlags", unitType, {ioType, i32Type});
    addFunction("ImGuiIO_AddConfigFlags", unitType, {ioType, i32Type});
    addFunction("ImGuiIO_ClearConfigFlags", unitType, {ioType, i32Type});
    addFunction("ImGuiIO_GetFramerate", f32Type, {ioType});
    addFunction("ImGuiIO_GetDeltaTime", f32Type, {ioType});
    addFunction("ImGuiIO_GetWantCaptureMouse", boolType, {ioType});
    addFunction("ImGuiIO_GetWantCaptureKeyboard", boolType, {ioType});
    addFunction("ImGuiIO_GetFontGlobalScale", f32Type, {ioType});
    addFunction("ImGuiIO_SetFontGlobalScale", unitType, {ioType, f32Type});

    addFunction("ImGuiStyle_ScaleAllSizes", unitType, {styleType, f32Type});
    addFunction("ImGuiStyle_GetAlpha", f32Type, {styleType});
    addFunction("ImGuiStyle_SetAlpha", unitType, {styleType, f32Type});
    addFunction("ImGuiStyle_GetWindowRounding", f32Type, {styleType});
    addFunction("ImGuiStyle_SetWindowRounding", unitType, {styleType, f32Type});
    addFunction("ImGuiStyle_GetFrameRounding", f32Type, {styleType});
    addFunction("ImGuiStyle_SetFrameRounding", unitType, {styleType, f32Type});

    addFunction("Begin", boolType, {stringType});
    addFunction("BeginFlags", boolType, {stringType, i32Type});
    addFunction("BeginChild", boolType, {stringType, f32Type, f32Type, boolType, i32Type});
    addFunction("SetNextWindowSize", unitType, {f32Type, f32Type, i32Type});
    addFunction("SetNextWindowPos", unitType, {f32Type, f32Type, i32Type});
    addFunction("SetNextWindowCollapsed", unitType, {boolType, i32Type});
    addFunction("SetNextWindowBgAlpha", unitType, {f32Type});
    addFunction("SetWindowCollapsed", unitType, {boolType, i32Type});
    addFunction("SetWindowFontScale", unitType, {f32Type});
    addFunction("SeparatorText", unitType, {stringType});
    addFunction("Dummy", unitType, {f32Type, f32Type});
    addFunction("Indent", unitType, {f32Type});
    addFunction("Unindent", unitType, {f32Type});
    addFunction("PushID", unitType, {stringType});
    addFunction("PushIDInt", unitType, {i32Type});
    addFunction("SetCursorPos", unitType, {f32Type, f32Type});
    addFunction("SetCursorPosX", unitType, {f32Type});
    addFunction("SetCursorPosY", unitType, {f32Type});
    addFunction("SetScrollX", unitType, {f32Type});
    addFunction("SetScrollY", unitType, {f32Type});
    addFunction("SetScrollHereX", unitType, {f32Type});
    addFunction("SetScrollHereY", unitType, {f32Type});
    addFunction("SetKeyboardFocusHere", unitType, {i32Type});
    addFunction("BeginDisabled", unitType, {boolType});
    addFunction("SetTooltip", unitType, {stringType});
    addFunction("SetItemTooltip", unitType, {stringType});
    addFunction("PushStyleColor", unitType, {i32Type, f32Type, f32Type, f32Type, f32Type});
    addFunction("PopStyleColor", unitType, {i32Type});
    addFunction("PushStyleVarFloat", unitType, {i32Type, f32Type});
    addFunction("PushStyleVarVec2", unitType, {i32Type, f32Type, f32Type});
    addFunction("PopStyleVar", unitType, {i32Type});

    for (const auto &name : Vec<Str>{"Text", "TextDisabled", "TextWrapped", "BulletText", "CollapsingHeader",
                                     "TreeNode", "BeginTabBar", "BeginTabItem", "BeginMenu", "OpenPopup",
                                     "BeginPopup"})
    {
      auto ret = (name == "Text" || name == "TextDisabled" || name == "TextWrapped" || name == "BulletText" ||
                  name == "OpenPopup")
                     ? unitType
                     : boolType;
      addFunction(name, ret, {stringType});
    }
    addFunction("LabelText", unitType, {stringType, stringType});
    addFunction("TextColored", unitType, {f32Type, f32Type, f32Type, f32Type, stringType});
    addFunction("TextNgHighlighted", unitType, {stringType});
    addFunction("Button", boolType, {stringType});
    addFunction("ButtonSized", boolType, {stringType, f32Type, f32Type});
    addFunction("SmallButton", boolType, {stringType});
    addFunction("Checkbox", boolType, {stringType, boolType});
    addFunction("RadioButton", boolType, {stringType, boolType});
    addFunction("SliderFloat", f32Type, {stringType, f32Type, f32Type, f32Type});
    addFunction("SliderInt", i32Type, {stringType, i32Type, i32Type, i32Type});
    addFunction("DragFloat", f32Type, {stringType, f32Type, f32Type, f32Type, f32Type});
    addFunction("DragInt", i32Type, {stringType, i32Type, f32Type, i32Type, i32Type});
    addFunction("InputText", stringType, {stringType, stringType, i32Type});
    addFunction("InputTextMultiline", stringType, {stringType, stringType, f32Type, f32Type, i32Type});
    addFunction("InputFloat", f32Type, {stringType, f32Type, f32Type, f32Type, i32Type});
    addFunction("InputInt", i32Type, {stringType, i32Type, i32Type, i32Type, i32Type});
    addFunction("Selectable", boolType, {stringType, boolType, i32Type, f32Type, f32Type});
    addFunction("BeginCombo", boolType, {stringType, stringType, i32Type});
    addFunction("ProgressBar", unitType, {f32Type, f32Type, f32Type, stringType});
    addFunction("BeginTable", boolType, {stringType, i32Type, i32Type, f32Type, f32Type, f32Type});
    addFunction("TableNextRow", unitType, {i32Type, f32Type});
    addFunction("TableSetColumnIndex", boolType, {i32Type});
    addFunction("TableSetupColumn", unitType, {stringType, i32Type, f32Type});
    addFunction("TableHeader", unitType, {stringType});
    addFunction("MenuItem", boolType, {stringType, boolType});
    addFunction("IsItemClicked", boolType, {i32Type});
    addFunction("IsMouseClicked", boolType, {i32Type});

    NG::module::get_module_registry().registerNativeModuleDescriptor(descriptor);
  };

  void register_vm_natives(NG::orgasm::VM &vm)
  {
    auto runtimeModule = make_runtime_module();
    bind_native_library_handlers(runtimeModule, handlers);
    for (auto &[name, handler] : runtime_module_native_functions(runtimeModule))
    {
      vm.register_native_raw(name, [handler](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
        auto env = make_runtime_env();
        bind_native_arg_slots(env, args);
        return handler(unit_cell(), env, args);
      });
    }
  }

  Vec<Str> native_function_names()
  {
    Vec<Str> names;
    names.reserve(handlers.size());
    for (auto &[name, _] : handlers)
    {
      names.push_back(name);
    }
    return names;
  }
} // namespace NG::library::imgui
