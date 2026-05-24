#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <intp/runtime.hpp>
#include <module.hpp>
#include <orgasm/vm.hpp>
#include <runtime/native_marshaling.hpp>
#include <filesystem>

namespace NG::library::imgui
{
  using namespace NG::runtime;
  using namespace NG::runtime::native;

  namespace
  {
    const Str IMGUI_STATE_KEY = "$$imgui.state$$";

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

    auto require_imgui_module(const NGEnv &context) -> RuntimeRef<StorageCell>
    {
      auto module = current_native_module(context);
      if (module == nullptr)
      {
        throw RuntimeException("imgui native call is missing bound module context");
      }
      return module;
    }

    auto find_imgui_state(const NGEnv &context) -> std::shared_ptr<ImGuiModuleState>
    {
      auto module = require_imgui_module(context);
      auto state = runtime_module_get_native_state(module, IMGUI_STATE_KEY);
      return state ? std::static_pointer_cast<ImGuiModuleState>(state) : nullptr;
    }

    auto require_imgui_state(const Str &functionName, const NGEnv &context) -> std::shared_ptr<ImGuiModuleState>
    {
      auto state = find_imgui_state(context);
      if (!state)
      {
        throw RuntimeException(functionName + "() requires imgui.init() before use");
      }
      return state;
    }

    void store_imgui_state(const NGEnv &context, const std::shared_ptr<ImGuiModuleState> &state)
    {
      runtime_module_set_native_state(require_imgui_module(context), IMGUI_STATE_KEY, state);
    }

    void clear_imgui_state(const NGEnv &context)
    {
      runtime_module_clear_native_state(require_imgui_module(context), IMGUI_STATE_KEY);
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
  } // namespace

  static Map<Str, NGCallable> handlers{
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
           ImGui::Begin(titleValue.c_str());
           return unit_cell();
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
  };

  void do_register()
  {
    register_native_library("std.imgui", handlers);
    using namespace NG::typecheck;
    auto unitType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
    auto boolType = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
    auto stringType = makecheck<PrimitiveType>(typeinfo_tag::STRING);

    auto descriptor = makert<NG::module::NativeModuleDescriptor>();
    descriptor->moduleId = "std.imgui";
    descriptor->functions = handlers;
    descriptor->exports.declared.insert("*");
    for (const auto &[name, _handler] : handlers)
    {
      descriptor->typeIndex.insert_or_assign(name, makecheck<FunctionType>(unitType, Vec<CheckingRef<TypeInfo>>{}));
    }
    descriptor->typeIndex.insert_or_assign("Begin", makecheck<FunctionType>(unitType, Vec<CheckingRef<TypeInfo>>{stringType}));
    descriptor->typeIndex.insert_or_assign("Text", makecheck<FunctionType>(unitType, Vec<CheckingRef<TypeInfo>>{stringType}));
    descriptor->typeIndex.insert_or_assign("Aborted", makecheck<FunctionType>(boolType, Vec<CheckingRef<TypeInfo>>{}));
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
