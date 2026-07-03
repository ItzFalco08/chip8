#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <random>
#include "glad/gl.h"
#include "SDL3/SDL.h"
#include <SDL3/SDL_opengl.h>
#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"

// globals
int scale = 10; // pixel scale
const int fb_x = 64;
const int fb_y = 32;

constexpr double cpuHz = 700.0;
constexpr double stdtHz = 60.0;
constexpr double cpuStep = 1.0 / cpuHz;
constexpr double stdtStep = 1.0 / stdtHz;


#define FONT_START 0x0

struct fontchar_t {
    uint8_t charData[5];
};

const fontchar_t sprites[] = {
    {0xF0, 0x90, 0x90, 0x90, 0xF0}, // 0
    {0x20, 0x60, 0x20, 0x20, 0x70}, // 1
    {0xF0, 0x10, 0xF0, 0x80, 0xF0}, // 2
    {0xF0, 0x10, 0xF0, 0x10, 0xF0}, // 3
    {0x90, 0x90, 0xF0, 0x10, 0x10}, // 4
    {0xF0, 0x80, 0xF0, 0x10, 0xF0}, // 5
    {0xF0, 0x80, 0xF0, 0x90, 0xF0}, // 6
    {0xF0, 0x10, 0x20, 0x40, 0x40}, // 7
    {0xF0, 0x90, 0xF0, 0x90, 0xF0}, // 8
    {0xF0, 0x90, 0xF0, 0x10, 0xF0}, // 9
    {0xF0, 0x90, 0xF0, 0x90, 0x90}, // A
    {0xE0, 0x90, 0xE0, 0x90, 0xE0}, // B
    {0xF0, 0x80, 0x80, 0x80, 0xF0}, // C
    {0xE0, 0x90, 0x90, 0x90, 0xE0}, // D
    {0xF0, 0x80, 0xF0, 0x80, 0xF0}, // E
    {0xF0, 0x80, 0xF0, 0x80, 0x80}  // F
};

void read_instructions(const char* path, uint8_t* fileData) {
    // read rom.ch8
    std::ifstream file(path, std::ios::binary);
    if(!file) {throw std::runtime_error("failed to open file");};
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > (4096 - 0x200)) throw std::runtime_error("[PANIC] Rom size exceeds max limit");

    file.read(reinterpret_cast<char*>(fileData), size);
}

struct chipstate_t {
    // cpu
    uint8_t registers[16] = {}; // V0 - VF registers
    uint16_t I {}; // index register

    uint8_t dt {}, st {};

    uint16_t stack[32] = {}; // 2 byte units (64 byte stack)
    uint8_t sp = 0; // stack pointer

    uint16_t pc = 0x200;

    // ram
    bool fb[fb_x * fb_y] = {}; // display ( 64x32 pixels )
    uint8_t memory[4096] = {}; // 4096 bytes of addressable memory ( 1 byte unit )

    // keyboard
    bool keys[16] = {};

private:
    uint8_t read8(uint16_t adr) {
        return memory[adr & 0xFFF];
    }

    void write8(uint16_t adr, uint8_t data) {
        memory[adr & 0xFFF] = data;
    }

public:
    void init(const char* rom) {
        read_instructions(rom, &memory[0x200]);
        memcpy(FONT_START + &memory[0], &sprites[0], 80);
    }

    uint16_t getopcode() {
        return memory[pc] << 8 | memory[pc + 1];
    }

    void step() {
        // run one instruction
        uint16_t opcode = getopcode();
        pc += 2;

        uint8_t op = opcode >> 12; // first nibble
        uint8_t x = (opcode >> 8) & 0xF; // second nibble
        uint8_t y = (opcode >> 4) & 0xF; // third nibble
        uint8_t n = opcode & 0xF; // fourth nibble
        uint8_t kk = opcode & 0xFF; // last 2 nibble
        uint16_t nnn = opcode & 0xFFF; // last 3 nibble

        switch (op)
        {
        case 0x0:
            if(y == 0xE && n == 0x0) {
                memset(fb, 0, sizeof(fb));
            } else if (y == 0xE && n == 0xE) {
                pc = stack[--sp];
            }
            break;
        
        case 0x1:
            pc = nnn;
            break;

        case 0x2:
            stack[sp++] = pc;
            pc = nnn;
            break;

        case 0x3:
            if (registers[x] == kk) pc+=2;
            break;

        case 0x4:
            if (registers[x] != kk) pc+=2;
            break;
        
        case 0x5:
            if(registers[x] == registers[y]) pc+=2;
            break;
        
        case 0x6:
            registers[x] = kk;
            break;

        case 0x7:
            registers[x] += kk;
            break;

        case 0x8: {

            uint8_t &vx = registers[x];
            uint8_t &vy = registers[y];

            if (n == 0x0) {
                vx = vy;
            } else if (n == 0x1) {
                vx |= registers[y];
            } else if (n == 0x2) {
                vx &= registers[y];
            } else if (n == 0x3) {
                vx ^= vy;
            } else if (n == 0x4) {
                int sum = vx + vy;
                registers[0xf] = sum > 0xFF;
                registers[x] = static_cast<uint8_t>(sum);
            } else if (n == 0x5) {
                registers[0xF] = vx >= vy;
                vx -= vy;
            } else if (n == 0x6) { 
                registers[0xF] = vx & 0x1;
                vx >>= 1;
            } else if (n == 0x7) {
                registers[0xF] = vy >= vx;
                vx = vy - vx;
            } else if (n == 0xE) {
                registers[0xF] = (vx >> 7);
                vx *= 2;
            }
            break;
        }

        case 0x9:
            if (registers[x] != registers[y]) pc+=2;
            break;

        case 0xA:
            I = nnn;
            break;

        case 0xB:
            pc = nnn + registers[0];        
            break;

        case 0xC: {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_int_distribution<int> distrib(0,255);
            uint8_t rndu8 = static_cast<uint8_t>(distrib(gen));
            registers[x] = rndu8 & kk;
            break;
        }
        case 0xD: {
            uint8_t& vx = registers[x];
            uint8_t& vy = registers[y];
            
            registers[0xf] = 0;
            for (int y = 0; y < n; ++y) {
                uint8_t src = read8(I + y);
                for (int x = 0; x < 8; x++) {
                    int px = (vx + x) % fb_x;
                    int py = (vy + y) % fb_y;

                    bool& fbbit = fb[px + py * fb_x];
                    uint8_t drawbit = ((src >> (7-x)) & 0x1);
                    
                    if(fbbit == 1 && drawbit == 1) registers[0xf] = 1;
                    fbbit ^= drawbit;
                }
            }
            break;
        }

        case 0xE:
            if (kk == 0x9E) {
                if(keys[registers[x]]) pc += 2;
            } else if (kk == 0xA1) {
                if(!keys[registers[x]]) pc += 2;
            }
            break;

        case 0xF:
            if (kk == 0x07) {
                registers[x] = dt;
            } else if (kk == 0x0A) {
                bool found = false;
                for(int i = 0; i < 16; ++i) {
                    if (keys[i]) {
                        registers[x] = i;
                        found = true;
                        break;
                    }
                }
                if(!found) pc -= 2; 
                break;
            } else if (kk == 0x15) {
                dt = registers[x];
            } else if (kk == 0x18) {
                st = registers[x];
            } else if (kk == 0x1E) {
                I += registers[x];
            } else if (kk == 0x29) {
                I = FONT_START + registers[x] * 5;
            } else if (kk == 0x33) {
                write8(I, registers[x] / 100);
                write8(I + 1, (registers[x] / 10) % 10);
                write8(I + 2, registers[x] % 10);
            } else if (kk == 0x55) {
                for (int i = 0; i <= x; ++i) {
                    write8(I + i, registers[i]);
                }
                I += x + 1;
            } else if (kk == 0x65) {
                for (int i = 0; i <= x; ++i) {
                    registers[i] = read8(I + i);
                }
                I += x + 1;
            }
            break;

        default:
            printf("failed to execute instruction %x", opcode);
            break;
        }
    };

};

struct gldisplaytexture_t  {
    uint32_t m_id = 0;
    int m_width = 0;
    int m_height = 0;

    gldisplaytexture_t() = default;
    gldisplaytexture_t(const gldisplaytexture_t&) = delete;
    gldisplaytexture_t& operator=(const gldisplaytexture_t&) = delete;

    gldisplaytexture_t(gldisplaytexture_t&& other) noexcept {
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
    }

    gldisplaytexture_t& operator=(gldisplaytexture_t&& other) noexcept {
        if (this == &other) return *this;
        if (m_id != 0) glDeleteTextures(1, &m_id);
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
        return *this;
    }

    gldisplaytexture_t(int width, int height): m_width(width), m_height(height) {
        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            m_width,
            m_height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void update(const uint32_t* buffer) {
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, buffer );
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~gldisplaytexture_t() {
        if (m_id != 0) glDeleteTextures(1, &m_id);
    }
};

class EmuApp {
    chipstate_t chipState{};
    SDL_GLContext glContext{};
    SDL_Window* window = nullptr;
    bool running = true;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    gldisplaytexture_t display_texture;

public:
    void run() {
        const char* rom = "Pong.ch8";
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

        display_texture = gldisplaytexture_t(fb_x, fb_y);
    }

    void init_imgui() {
        // imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        init_imgui_theme();

        // backends
        ImGui_ImplSDL3_InitForOpenGL(window, glContext);
        ImGui_ImplOpenGL3_Init("#version 330 core");
    }

    void main_loop() noexcept {
        while (running) {
            // sdl events
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);

                switch (event.type)
                {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                
                case SDL_EVENT_KEY_DOWN: {
                    int k_code = mapKey(event.key.key);
                    if(k_code != -1) {
                        chipState.keys[k_code] = true;
                    }
                    break;
                }

                case SDL_EVENT_KEY_UP: {
                    int k_code = mapKey(event.key.key);
                    if (k_code != -1) {
                        chipState.keys[k_code] = false;                        
                    }
                    break;
                }
                default:
                    break;
                }
            }

            glClear(GL_COLOR_BUFFER_BIT);
            
            // imgui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            // draw
            execute_program();
            draw_imgui();
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            SDL_GL_SwapWindow(window);
        }

        SDL_GL_DestroyContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    // helpers
    double last = 0.0;

    double cpu_accumulated_time = 0.0;
    double stdt_accumulated_time = 0.0;

    void execute_program() {
        if (last == 0.0) last = SDL_GetTicks() / 1000.0;
        double cur = SDL_GetTicks() / 1000.0;
        double dt = cur - last;
        last = cur;

        // instructions
        cpu_accumulated_time += dt;
        while (cpu_accumulated_time > cpuStep) {
            chipState.step();
            cpu_accumulated_time -= cpuStep;
        }

        // dt & st
        stdt_accumulated_time += dt;
        while (stdt_accumulated_time > stdtStep) {
            if (chipState.st > 0) chipState.st--;
            if (chipState.dt > 0) chipState.dt--;
            stdt_accumulated_time -= stdtStep;
        }
    }

    void draw_imgui() {        
        int display_w = scale * fb_x;
        int display_h = scale * fb_y;

        // draw chip8 framebuffer as ImGui::Image
        uint32_t fbRGBA[fb_x * fb_y];
        for (int i = 0; i < fb_x * fb_y; ++i) {
            fbRGBA[i] = chipState.fb[i] ? 0xFFFFFFFF : 0xFF000000;
        }
        display_texture.update(&fbRGBA[0]);
        
        ImGui::SetNextWindowSize(ImVec2(display_w, display_h + ImGui::GetFrameHeight()));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Display", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::PopStyleVar();
        ImGui::Image((ImTextureID)(intptr_t)display_texture.m_id, ImVec2(scale * fb_x, display_h));
        ImGui::End();

        // debug
        ImGui::Begin("Debug"); // 121a 3000 f007
        ImGui::Text("pc: %i", chipState.pc);
        ImGui::Text("op: %x", chipState.getopcode());
        for (int i = 0; i <= 0xF; ++i) {
            ImGui::Text("V%i: %i", i, chipState.registers[i]);
        }
        ImGui::End();
    };

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

    int mapKey(SDL_Keycode key)
    {
        switch (key)
        {
            case SDLK_1: return 0x1;
            case SDLK_2: return 0x2;
            case SDLK_3: return 0x3;
            case SDLK_4: return 0xC;

            case SDLK_Q: return 0x4;
            case SDLK_W: return 0x5;
            case SDLK_E: return 0x6;
            case SDLK_R: return 0xD;

            case SDLK_A: return 0x7;
            case SDLK_S: return 0x8;
            case SDLK_D: return 0x9;
            case SDLK_F: return 0xE;

            case SDLK_Z: return 0xA;
            case SDLK_X: return 0x0;
            case SDLK_C: return 0xB;
            case SDLK_V: return 0xF;

            default: return -1;
        }
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