#include <cmath>
#include <iostream>
#include <numbers>

#define SDL_MAIN_USE_CALLBACKS 1  // Tell SDL to use the callback architecture
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>  // Emscripten provides WebGL2 symbols natively here
#include <emscripten.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#endif

// Simple matrix math helpers (to avoid external dependencies like GLM for this
// example)
void multiply_matrix(const float* a, const float* b, float* out) {
  float res[16];
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      res[i * 4 + j] = 0;
      for (int k = 0; k < 4; ++k) {
        res[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
      }
    }
  }
  for (int i = 0; i < 16; ++i) out[i] = res[i];
}

void get_rotation_y(float angle, float* m) {
  float c = cosf(angle), s = sinf(angle);
  float r[16] = {
      c,  0, s, 0,  // X
      0,  1, 0, 0,  // Y
      -s, 0, c, 0,  // Z
      0,  0, 0, 1   // W
  };
  for (int i = 0; i < 16; ++i) m[i] = r[i];
}

void get_projection(float fov, float aspect, float nearZ, float farZ,
                    float* m) {
  float f = 1.0f / tanf(fov / 2.0f);
  for (int i = 0; i < 16; i++) m[i] = 0.0f;
  m[0] = f / aspect;
  m[5] = f;
  m[10] = (farZ + nearZ) / (nearZ - farZ);
  m[11] = (2.0f * farZ * nearZ) / (nearZ - farZ);
  m[14] = -1.0f;
}

// Shader Sources
#ifdef __EMSCRIPTEN__
const char* vertexShaderSource = R"(#version 300 es
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 ourColor;
    uniform mat4 uMVP;
    void main() {
        gl_Position = uMVP * vec4(aPos, 1.0);
        ourColor = aColor;
    }
)";

const char* fragmentShaderSource = R"(#version 300 es
    precision mediump float; // <-- MANDATORY in WebGL 2 fragment shaders
    in vec3 ourColor;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(ourColor, 1.0);
    }
)";
#else
const char* vertexShaderSource = R"(#version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 ourColor;
    uniform mat4 uMVP;
    void main() {
        gl_Position = uMVP * vec4(aPos, 1.0);
        ourColor = aColor;
    }
)";

const char* fragmentShaderSource = R"(#version 330 core
    in vec3 ourColor;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(ourColor, 1.0);
    }
)";
#endif

// Helper to compile and check shaders
GLuint compileShader(GLenum type, const char* source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char infoLog[512];
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    std::cerr << "SHADER COMPILATION ERROR ("
              << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << "):\n"
              << infoLog << std::endl;
  }
  return shader;
}

// Main code
// A struct to hold your application state (replaces global variables)
struct AppState {
  SDL_Window* window = nullptr;
  SDL_GLContext gl_context = nullptr;

  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
  ImGuiIO* io;

  GLuint shaderProgram;
  GLuint VAO, VBO, EBO;
  GLint mvpLoc;
};

// 1. Called once when the app starts. Initialize everything here.
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
  // Allocate our custom state structure
  AppState* state = new AppState();
  *appstate = state;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    SDL_Log("SDL Initialization failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

#ifdef __EMSCRIPTEN__
  // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
  const char* glsl_version = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
  // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
  // GL ES 2.0 + GLSL 100 (WebGL 1.0)
  const char* glsl_version = "#version 100";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
  // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
  const char* glsl_version = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
  // GL 3.2 Core + GLSL 150
  const char* glsl_version = "#version 150";
  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_FLAGS,
      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);  // Always required on Mac
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
  // GL 3.0 + GLSL 130
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
#endif
  std::cout << "IMGUI IMPL: " << glsl_version << std::endl;

  // Create window with graphics context
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                 SDL_WINDOW_HIDDEN |
                                 SDL_WINDOW_HIGH_PIXEL_DENSITY;

  state->window =
      SDL_CreateWindow("SDL3+OpenGL3 example", (int)(1280 * main_scale),
                       (int)(800 * main_scale), window_flags);
  if (state->window == nullptr) {
    SDL_Log("Window creation failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  state->gl_context = SDL_GL_CreateContext(state->window);
  if (state->gl_context == nullptr) {
    SDL_Log("OpenGL Context creation failed: %s", SDL_GetError());
    SDL_DestroyWindow(state->window);
    return SDL_APP_FAILURE;
  }

  // Enable Depth Testing for 3D
  glEnable(GL_DEPTH_TEST);

  // SHADER SETUP START
  // Compile and Link Shaders
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
  state->shaderProgram = glCreateProgram();
  glAttachShader(state->shaderProgram, vs);
  glAttachShader(state->shaderProgram, fs);
  glLinkProgram(state->shaderProgram);
  {
    GLint success;
    glGetProgramiv(state->shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetProgramInfoLog(state->shaderProgram, 512, NULL, infoLog);
      std::cerr << "SHADER PROGRAM LINKING ERROR:\n" << infoLog << std::endl;
    }
  }
  glDeleteShader(vs);
  glDeleteShader(fs);

  // Cube Vertices: Position (X,Y,Z) and Color (R,G,B)
  float vertices[] = {
      // Front face          // Colors
      -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f,   // BL
      0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f,    // BR
      0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f,     // TR
      -0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f,    // TL
                                              // Back face
      -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f,  // BL
      0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f,   // BR
      0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 1.0f,    // TR
      -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 0.0f    // TL
  };

  unsigned int indices[] = {
      0, 1, 2, 2, 3, 0,  // Front
      1, 5, 6, 6, 2, 1,  // Right
      7, 6, 5, 5, 4, 7,  // Back
      4, 0, 3, 3, 7, 4,  // Left
      4, 5, 1, 1, 0, 4,  // Bottom
      3, 2, 6, 6, 7, 3   // Top
  };

  glGenVertexArrays(1, &state->VAO);
  glGenBuffers(1, &state->VBO);
  glGenBuffers(1, &state->EBO);

  glBindVertexArray(state->VAO);
  glBindBuffer(GL_ARRAY_BUFFER, state->VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);

  // Position Attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);
  // Color Attribute
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  state->mvpLoc = glGetUniformLocation(state->shaderProgram, "uMVP");
  // SHADER SETUP END

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  state->io = &ImGui::GetIO();
  state->io->ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  state->io->ConfigFlags |=
      ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup scaling
  ImGuiStyle& style = ImGui::GetStyle();
  style.ScaleAllSizes(
      main_scale);  // Bake a fixed style scale. (until we have a solution for
                    // dynamic style scaling, changing this requires resetting
                    // Style + calling this again)
  style.FontScaleDpi =
      main_scale;  // Set initial font scale. (in docking branch: using
                   // io.ConfigDpiScaleFonts=true automatically overrides this
                   // for every window depending on the current monitor)

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForOpenGL(state->window, state->gl_context);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // Load Fonts
  // - If fonts are not explicitly loaded, Dear ImGui will select an embedded
  // font: either AddFontDefaultVector() or AddFontDefaultBitmap().
  //   This selection is based on (style.FontSizeBase * style.FontScaleMain *
  //   style.FontScaleDpi) reaching a small threshold.
  // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select
  // them.
  // - If a file cannot be loaded, AddFont functions will return a nullptr.
  // Please handle those errors in your code (e.g. use an assertion, display an
  // error and quit).
  // - Read 'docs/FONTS.md' for more instructions and details.
  // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType
  // for higher quality font rendering.
  // - Remember that in C/C++ if you want to include a backslash \ in a string
  // literal you need to write a double backslash \\ !
  // - Our Emscripten build process allows embedding fonts to be accessible at
  // runtime from the "fonts/" folder. See Makefile.emscripten for details.
  // style.FontSizeBase = 20.0f;
  // state->io->Fonts->AddFontDefaultVector();
  // state->io->Fonts->AddFontDefaultBitmap();
  // state->io->Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
  // state->io->Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
  // state->io->Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
  // state->io->Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
  // ImFont* font =
  // state->io->Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
  // IM_ASSERT(font != nullptr);

#ifdef __EMSCRIPTEN__
  // For an Emscripten build we are disabling file-system access, so let's not
  // attempt to do a fopen() of the imgui.ini file. You may manually call
  // LoadIniSettingsFromMemory() to load settings from your own storage.
  state->io->IniFilename = nullptr;
#endif

  SDL_GL_MakeCurrent(state->window, state->gl_context);
  SDL_GL_SetSwapInterval(1);  // Enable vsync
  SDL_SetWindowPosition(state->window, SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(state->window);

  return SDL_APP_CONTINUE;  // Tells SDL to keep running
}

// 2. Called whenever the system fires an event (input, window resize, etc.)
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
  AppState* state = static_cast<AppState*>(appstate);
  ImGui_ImplSDL3_ProcessEvent(event);
  if (event->type == SDL_EVENT_QUIT) {
    return SDL_APP_SUCCESS;  // Safely exits the application
  }
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
      event->window.windowID == SDL_GetWindowID(state->window)) {
    return SDL_APP_SUCCESS;  // Safely exits the application
  }
  if (event->type == SDL_EVENT_KEY_DOWN) {
    if (event->key.scancode == SDL_SCANCODE_ESCAPE) {
      return SDL_APP_SUCCESS;
    }
  }
  return SDL_APP_CONTINUE;
}

// 3. Called every single frame. This is your update and render loop.
SDL_AppResult SDL_AppIterate(void* appstate) {
  AppState* state = static_cast<AppState*>(appstate);

  if (SDL_GetWindowFlags(state->window) & SDL_WINDOW_MINIMIZED) {
    SDL_Delay(10);
    return SDL_APP_CONTINUE;  // Keep looping
  }

  // --- 1. Update Game/App Logic Here ---
  float time = SDL_GetTicks() / 1000.0f;

  // Build MVP Matrix
  float model[16];
  get_rotation_y(time, model);  // Rotate around Y axis over time

  float view[16] = {
      1, 0, 0, 0,      // X
      0, 1, 0, 0,      // Y
      0, 0, 1, -2.5f,  // Z Move back 2.5 units on Z
      0, 0, 0, 1       // W
  };

  float projection[16];
  get_projection(45.0f * (std::numbers::pi / 180.0f), 1280.0f / 800.0f, 0.1f,
                 100.0f, projection);

  float viewProj[16];
  multiply_matrix(projection, view, viewProj);
  float mvp[16];
  multiply_matrix(viewProj, model, mvp);

  // Pass matrix to shader
  glUseProgram(state->shaderProgram);
  glUniformMatrix4fv(state->mvpLoc, 1, GL_TRUE, mvp);

  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  // 1. Show the big demo window (Most of the sample code is in
  // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
  // ImGui!).
  if (state->show_demo_window) ImGui::ShowDemoWindow(&state->show_demo_window);

  // 2. Show a simple window that we create ourselves. We use a Begin/End pair
  // to create a named window.
  {
    static float f = 0.0f;
    static int counter = 0;

    ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!"
                                    // and append into it.

    ImGui::Text("This is some useful text.");  // Display some text (you can use
                                               // a format strings too)
    ImGui::Checkbox("Demo Window",
                    &state->show_demo_window);  // Edit bools storing our window
                                                // open/close state
    ImGui::Checkbox("Another Window", &state->show_another_window);

    ImGui::SliderFloat("float", &f, 0.0f,
                       1.0f);  // Edit 1 float using a slider from 0.0f to 1.0f
    ImGui::ColorEdit3(
        "clear color",
        (float*)&state->clear_color);  // Edit 3 floats representing a color

    if (ImGui::Button("Button"))  // Buttons return true when clicked (most
                                  // widgets return true when edited/activated)
      counter++;
    ImGui::SameLine();
    ImGui::Text("counter = %d", counter);

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / state->io->Framerate, state->io->Framerate);
    ImGui::End();
  }

  // 3. Show another simple window.
  if (state->show_another_window) {
    ImGui::Begin(
        "Another Window",
        &state->show_another_window);  // Pass a pointer to our bool variable
                                       // (the window will have a closing button
                                       // that will clear the bool when clicked)
    ImGui::Text("Hello from another window!");
    if (ImGui::Button("Close Me")) state->show_another_window = false;
    ImGui::End();
  }

  // --- 2. Render Graphics Here ---
  ImGui::Render();
  glViewport(0, 0, (int)state->io->DisplaySize.x,
             (int)state->io->DisplaySize.y);
  glClearColor(state->clear_color.x * state->clear_color.w,
               state->clear_color.y * state->clear_color.w,
               state->clear_color.z * state->clear_color.w,
               state->clear_color.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Draw Cube
  glBindVertexArray(state->VAO);
  glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);

  // Draw Imgui
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  SDL_GL_SwapWindow(state->window);

  return SDL_APP_CONTINUE;  // Keep looping
}

// 4. Called once automatically right before the program terminates
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
  if (appstate) {
    AppState* state = static_cast<AppState*>(appstate);

    glDeleteVertexArrays(1, &state->VAO);
    glDeleteBuffers(1, &state->VBO);
    glDeleteBuffers(1, &state->EBO);
    glDeleteProgram(state->shaderProgram);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(state->gl_context);
    SDL_DestroyWindow(state->window);
    delete state;  // Clean up our heap memory
  }
  SDL_Quit();
  SDL_Log("Application shut down clean with code: %d", result);
}
