#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include "glad/gl.h"
#include "SDL3/SDL.h"
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"

// globals
int scale = 20; // pixel scale
constexpr size_t fb_x = 64;
constexpr size_t fb_y = 32;

constexpr uint8_t sprites[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

void read_instructions(const char* path, uint8_t* fileData) {
    // read rom.ch8
    std::ifstream file(path, std::ios::binary);
    if(!file) {std::cout << "failed to open file\n" << std::endl;};
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > (4096 - 0x200)) {
        std::cout << "[PANIC] Rom size exceeds max limit";
        return;
    }

    file.read(reinterpret_cast<char*>(fileData), size);
}

class chipstate_t {
    // cpu
    uint8_t registers[16] = {}; // V0 - VF registers
    uint16_t index {}; // index register

    uint8_t dt {}, st {};

    uint16_t stack[32] = {}; // 2 byte units (64 byte stack)
    uint8_t sp = 0; // stack pointer

    uint16_t pc = 0x200;

    // ram
    bool fb[fb_x * fb_y] = {}; // display ( 64x32 pixels )
    uint8_t memory[4096] = {}; // 4096 bytes of addressable memory ( 1 byte unit )

public:
    uint8_t read8(uint16_t adr) {
        return memory[adr & 0xFFF];
    }

    void write8(uint16_t adr, uint8_t data) {
        memory[adr & 0xFFF] = data;
    }

    void run(std::vector<uint8_t>& instructions) {
        for(uint8_t& byte: instructions) {
            printf("%x ", byte);
        }
        printf("\n\n Nr Bytes: %i", instructions.size());
    };

    void init(const char* rom) {
        read_instructions(rom, &memory[0x200]);
        memcpy(&memory[0], &sprites[0], 80);
    }
};

class EmuApp {
    chipstate_t chipState{};
    SDL_GLContext glContext{};
    SDL_Window* window = nullptr;
    ImGuiIO io;
    bool running = true;
    bool show_demo_window = true;
    bool show_another_window = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
public:
    void run() {
        const char* rom = "rom.ch8";
        chipState.init(rom);

        init_window();
        init_imgui();
        main_loop();
    };
private:

    void init_window() {
        // SDL + OpenGL init
        if (!SDL_Init(SDL_INIT_VIDEO)) throw SDL_GetError();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);

        window = SDL_CreateWindow( "chip8 emulator", 1024, 512, SDL_WINDOW_OPENGL );

        if (!window) {
            SDL_Quit();
            throw SDL_GetError();            
        }

        glContext = SDL_GL_CreateContext(window);

        if (!glContext)
        {
            SDL_DestroyWindow(window);
            SDL_Quit();
            throw SDL_GetError();
        }

        SDL_GL_MakeCurrent(window, glContext);

        // glad
        if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) { throw "Failed to initialize GLAD"; }

    }
    void init_imgui() {
        // imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        init_imgui_theme();

        // backends    
        ImGui_ImplSDL3_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 300 es");
    }
    void main_loop() noexcept {
        while (running) {
            // sdl events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                if (event.type == SDL_EVENT_QUIT) running = false;
            }

            glClear(GL_COLOR_BUFFER_BIT);
            
            // imgui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // draw
            ImGui::ShowDemoWindow(&show_demo_window);
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            SDL_GL_SwapWindow(window);
        }

        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    // helpers
    void init_imgui_theme() {
        // Excellency style by gonzaloivan121 from ImThemes
        ImGuiStyle& style = ImGui::GetStyle();
        
        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.WindowRounding = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.WindowMinSize = ImVec2(32.0f, 32.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 6.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 6.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(8.0f, 6.0f);
        style.FrameRounding = 6.0f;
        style.FrameBorderSize = 1.0f;
        style.ItemSpacing = ImVec2(6.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
        style.CellPadding = ImVec2(4.0f, 2.0f);
        style.IndentSpacing = 11.0f;
        style.ColumnsMinSpacing = 6.0f;
        style.ScrollbarSize = 14.0f;
        style.ScrollbarRounding = 6.0f;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 6.0f;
        style.TabRounding = 6.0f;
        style.TabBorderSize = 1.0f;
        style.TabMinWidthBase = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);
        
        style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.5019608f, 0.5019608f, 0.5019608f, 1.0f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08235294f, 0.08235294f, 0.08235294f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.15686275f, 0.15686275f, 0.15686275f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.19607843f, 0.19607843f, 0.19607843f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(0.101960786f, 0.101960786f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.05882353f, 0.05882353f, 0.05882353f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.09019608f, 0.09019608f, 0.09019608f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.05882353f, 0.05882353f, 0.05882353f, 1.0f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.08235294f, 0.08235294f, 0.08235294f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08235294f, 0.08235294f, 0.08235294f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15294118f, 0.15294118f, 0.15294118f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.019607844f, 0.019607844f, 0.019607844f, 0.53f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30980393f, 0.30980393f, 0.30980393f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.4117647f, 0.4117647f, 0.4117647f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.50980395f, 0.50980395f, 0.50980395f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.7529412f, 0.7529412f, 0.7529412f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.50980395f, 0.50980395f, 0.50980395f, 0.7f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.65882355f, 0.65882355f, 0.65882355f, 1.0f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.21960784f, 0.21960784f, 0.21960784f, 0.784f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.27450982f, 0.27450982f, 0.27450982f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.21960784f, 0.21960784f, 0.21960784f, 0.588f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.18431373f, 0.18431373f, 0.18431373f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.18431373f, 0.18431373f, 0.18431373f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.18431373f, 0.18431373f, 0.18431373f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.101960786f, 0.101960786f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.15294118f, 0.7254902f, 0.9490196f, 0.588f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.15294118f, 0.7254902f, 0.9490196f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.9098039f, 0.9098039f, 0.9098039f, 0.25f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.8117647f, 0.8117647f, 0.8117647f, 0.67f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.45882353f, 0.45882353f, 0.45882353f, 0.95f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.08235294f, 0.08235294f, 0.08235294f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(1.0f, 0.88235295f, 0.5294118f, 0.118f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(1.0f, 0.88235295f, 0.5294118f, 0.235f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08235294f, 0.08235294f, 0.08235294f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(1.0f, 0.88235295f, 0.5294118f, 0.118f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.6117647f, 0.6117647f, 0.6117647f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.0f, 0.43137255f, 0.34901962f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.9019608f, 0.7019608f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.0f, 0.6f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.18431373f, 0.18431373f, 0.18431373f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.30980393f, 0.30980393f, 0.34901962f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.101960786f, 0.101960786f, 0.101960786f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.15294118f, 0.7254902f, 0.9490196f, 0.35f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(1.0f, 1.0f, 0.0f, 0.9f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.15294118f, 0.7254902f, 0.9490196f, 0.8f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 1.0f, 1.0f, 0.7f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.8f, 0.8f, 0.8f, 0.35f);
    }
};

int main() {
    EmuApp app;

    try {
        app.run();
    } catch(std::runtime_error err) {
        std::cerr << err.what() << std::endl;
    }
    return 0;
}