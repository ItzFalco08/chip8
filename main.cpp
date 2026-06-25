#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>


constexpr uint16_t sprites[] = {
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

class chipstate_t {
    // cpu
    uint8_t registers[16]; // V0 - VF registers
    uint16_t I; // index register

    uint8_t delay_timer;
    uint8_t sound_timer;

    uint16_t stack[32]; // 2 byte units (64 byte stack)
    uint8_t sp = 0; // stack pointer

    uint16_t program_counter = 0x200;

    // ram
    // display ( 64x32 pixels )
    bool arr[64 * 32];
    // 4096 bytes of addressable memory ( 1 byte unit )
    uint8_t memory[4096];
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
};

void read_instructions(const char* path, std::vector<uint8_t>& fileData) {
    // read rom.ch8
    std::ifstream file("rom.ch8", std::ios::binary);
    if(!file) {std::cout << "failed to open file\n" << std::endl;};
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    fileData.resize(size + 1);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(fileData.data()), size);
}

int main() {
    chipstate_t chipState;
    
    std::vector<uint8_t> fileData;
    const char* fileName = "rom.ch8";
    read_instructions("rom.ch8", fileData);

    chipState.run(fileData);

    return 0;
}