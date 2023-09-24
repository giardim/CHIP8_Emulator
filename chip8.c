#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "SDL.h"

//https://github.com/kripod/chip8-roms

//SDL object
typedef struct{
    SDL_Window *window;
    SDL_Renderer *renderer;
}sdl_t;


//Configuration object
typedef struct{
    uint32_t screenWidth;   //Chip8 width
    uint32_t screenHeight;  //Chip8 height
    uint32_t fgColor;       //RGBA 8888
    uint32_t bgColor;       //RGBA 8888
    uint32_t scaleFactor;   //Scale the size of a Chip8 pixel
}config_t;

//Opcode format
typedef struct{
    uint16_t opcode;
    uint16_t NNN;       //12 bit address after the highest bit
    uint8_t NN;         //8 bit address after the highest bit
    uint8_t N;          //Constant 4 bit N
    uint8_t X;          //Constant 4 register ID X
    uint8_t Y;          //Constant 4 register ID Y
}instruction_t;

//Chip8 states
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
}state_flags_t;

//machine object
typedef struct{
    state_flags_t state;

    uint32_t ram[4096];     //Chip8 had 4096 memory locations, each a byte
    bool display[64*32];    //check if each pixel is on or off
    uint16_t PC;            //Program counter
    uint16_t stack[12];     //Subroutine stack -> 12 levels of nesting
    uint16_t *stack_ptr;    //Points to the stack and keeps track of how many subroutines are in the stack
    uint8_t V[16];          //16 registers, V0 - VF each a byte
    uint16_t I;             //Instruction register
    //Both timers decrement at 60hz
    uint8_t delay_timer;    //Delay timer
    uint8_t sound_timer;    //Sound timer
    bool keypad[16];        //hex keyboard, 0-F
    const char* rom_name;         //Name of rom currently loaded
    instruction_t inst;     //currently executing instruction
}chip8_t;

//Set config settings
bool set_config_setting(config_t *config, const int argc, char **argv){
    //DEFAULTS
    config -> screenWidth = 64;     //Default chip8 width
    config -> screenHeight = 32;    //Default chip8 height

    config -> fgColor = 0xFFFFFFFF; //Color of pixels, chip8 was all white
    config -> bgColor = 0x00000000; //Color of background, Chip8 was all black
    config -> scaleFactor = 20;     //Default resolution 1280x640

    for (int i = 1; i < argc; ++i){
        SDL_Log("%s\n", argv[i]);
    }
    return true;
}

//Run function on start up to initialize SDL dependencies
bool init_sdl(sdl_t *sdl, const config_t config){
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Failed to initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    const char* title = "Chip-8";

    sdl->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   config.screenWidth * config.scaleFactor,
                                   config.screenHeight * config.scaleFactor,
                                   0);

    if (!sdl -> window){
        SDL_Log("Could not create window: %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);

    if (!sdl->renderer){
        SDL_Log("Could not render window: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool init_chip8(chip8_t *chip8, const char rom_name[]){
    const uint16_t entry_point = 0x0200;

    const uint8_t font[] = {
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

    //load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    //load rom
    FILE *rom = fopen(rom_name, "rb");
    if (!rom){
        SDL_Log("Could not load rom: %s\n", SDL_GetError());
        return false;
    }

    //check rom size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size){
        SDL_Log("Rom is too big, exceeds max size: %s", SDL_GetError());
        return false;
    }

    //Read into memory
    if(fread(&chip8->ram[entry_point],rom_size, 1, rom) != 1){
        SDL_Log("Could not read rom file: %s\n", SDL_GetError());
        return false;
    };

    //Close file to prevent memory leaks
    fclose(rom);

    //Default
    chip8 -> state = RUNNING;
    chip8 -> rom_name = rom_name;
    chip8 -> PC = entry_point;             //Where chip8 programs are loaded
    printf("Entry point %X\n", chip8->PC);
    chip8 -> stack_ptr = &chip8->stack[0];
    return true;
}

//Run function on shutdown to clean up memory
void final_closedown(sdl_t *sdl){
    SDL_DestroyRenderer(sdl -> renderer);
    SDL_DestroyWindow(sdl -> window);
    SDL_Quit();
}

//set window to the starting colors
void clear_window(const config_t config, sdl_t sdl){
    uint8_t r = (config.bgColor >> 24) & 0xFF;
    uint8_t g = (config.bgColor >> 16) & 0xFF;
    uint8_t b = (config.bgColor >> 8) & 0xFF;
    uint8_t a = config.bgColor & 0xFF;
    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_window(sdl_t sdl, const config_t config, const chip8_t chip8){
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scaleFactor, .h = config.scaleFactor};

    //Set colors
    uint8_t fg_r = (config.fgColor >> 24) & 0xFF;
    uint8_t fg_g = (config.fgColor >> 16) & 0xFF;
    uint8_t fg_b = (config.fgColor >> 8) & 0xFF;
    uint8_t fg_a = config.fgColor & 0xFF;

    uint8_t bg_r = (config.bgColor >> 24) & 0xFF;
    uint8_t bg_g = (config.bgColor >> 16) & 0xFF;
    uint8_t bg_b = (config.bgColor >> 8) & 0xFF;
    uint8_t bg_a = config.bgColor & 0xFF;


    for (uint32_t i = 0; i < sizeof chip8.display; ++i){
        rect.x = (i % config.screenWidth) * config.scaleFactor;
        rect.y = (i / config.screenWidth) * config.scaleFactor;

        //If the pixel is on, draw the foreground color, else draw the background color
        if (chip8.display[i]){
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }else{
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

void get_input(chip8_t *chip8){
    SDL_Event e;
    while (SDL_PollEvent(&e)){
        switch(e.type){
            case SDL_KEYDOWN:
                switch(e.key.keysym.sym){
                    case(SDLK_ESCAPE):
                        chip8->state = QUIT;
                        break;
                    case(SDLK_SPACE):
                        if (chip8->state == RUNNING){
                            puts("=====PAUSED=====");
                            chip8->state = PAUSED;
                        }else{
                            puts("=====RUNNING=====");
                            chip8->state = RUNNING;
                        }
                        break;
                    default:
                    break;
                }
                break;
            case SDL_QUIT:
                chip8 -> state = QUIT;
                break;
            default:
                break;
        }
    }
    return;
}

#ifdef DEBUG
    void print_debug_info(chip8_t *chip8){
        printf("Address: 0x%04X, Opcode: 0x%04X, Desc: ", chip8->PC - 2, chip8->inst.opcode);
        switch (chip8->inst.opcode >> 8){
        case(0x00):
            if(chip8->inst.NN == 0xE0){
                //Clear the screen
                printf("Clear the screen\n");
            }
            else if (chip8->inst.NN == 0xEE){
                //return from subroutine
                printf("Returns from subroutine to address: 0x%04X\n", *(chip8->stack_ptr - 1));
            }
            else {
                printf("Error\n");
            }
            break;
        case(0x02):
            //call Subroutine
            //Put current program in the stack so we can return to it later, then set the
            //  program counter to the last 12bits in the opcode (NNN)
            printf("Call subroutine\n");
            break;
        case(0x06):
            //Set VX to NN
            printf("Set V[%01X] to NN(0x%02X)\n", chip8->inst.X, chip8->inst.NN);
            break;
        case(0x07):
            //Set VX to NN
            printf("Set V[%01X] += NN(0x%02X)\n", chip8->inst.X, chip8->inst.NN);
            break;
        case(0x0A):
            //Set register I to NNN
            printf("Set I to NNN (0x%04X)\n", chip8->inst.NNN);
            break;
        case(0x0D):
            printf("Drawing N (%u) height sprite at coords V%X (0x%02X) V%X (0x%02X) from memory location I (0x%04X)."
                " Set VF = 1 if any pixels are turned off\n", chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
            break;
        default:
            printf("unimplemented opcode\n");
            break;

    }
}
#endif

void emulate_instruction(chip8_t *chip8, config_t *config){
    //Get opcode
    chip8->inst.opcode = (chip8->ram[chip8->PC]) << 8 | chip8->ram[chip8->PC];
    printf("0x%04X\n", chip8->inst.opcode);
    chip8->PC += 2; //increment for next opcode

    chip8->inst.NNN = chip8->inst.opcode >> 8 & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode >> 8 & 0x0FF;
    chip8->inst.N = chip8->inst.opcode >> 8 & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;
     printf("0x%04X\n", chip8->inst.NNN);

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    //printf("0x%04X\n", chip8->inst.opcode);
    //Emulate opcode
    switch (chip8->inst.opcode){

        case(0x00):
            if(chip8->inst.NN == 0xE0){
                //Clear the screen
                memset(&chip8->display, false, sizeof chip8->display);
            }
            else if (chip8->inst.NN == 0xEE){
                //return from subroutine
                chip8 -> PC = *--chip8 -> stack_ptr;
            }
            break;
        case(0x02):
            //call Subroutine
            //Put current program in the stack so we can return to it later, then set the
            //  program counter to the last 12bits in the opcode (NNN)
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;
        case(0x06):
            //Set VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;
        case(0x07):
            //Set VX += NN
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;
        case(0x0A):
            //Set register I to NNN
            chip8->I = chip8->inst.NNN;
            break;
        case(0x0D):
            //Draw sprite at coordinate VX, VY, height of N, read from I
            //VF is set
            uint8_t X = chip8->V[chip8->inst.X] % config->screenWidth;
            uint8_t Y = chip8->V[chip8->inst.Y] % config->screenHeight;
            const uint8_t original_X = X;
            chip8->V[0xF] = 0;

            for(uint8_t i = 0; i < chip8->inst.N; ++i){
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X = original_X;
                for (int8_t j = 7; j >= 0; --j){
                    //If the bit in the spite and the bit on the display is on, set the carry
                    //  Flag to 1
                    bool *pixel = &chip8->display[Y * config->screenHeight + X];
                    const bool sprite_bit = (sprite_data & (1 << j));
                    if (sprite_bit && *pixel){
                        chip8->V[0xF] = 1;
                    }
                    //set the pixel on or off
                    *pixel ^= sprite_bit;

                    if (++X >= config->screenWidth){
                        break;
                    }
                }
                if (++Y >= config->screenHeight){
                    break;
                }
            }
            break;
        default:
            // puts("ERROR: INVALID OPCODE PRESENT");
            break;

    }
}

int main (int argc, char **argv){
    if (argc<2){
        fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    //set chip8 to the running state
    chip8_t chip8 = {0};
    const char* rom_name = argv[1];
    if (!init_chip8(&chip8, rom_name)){
        exit(EXIT_FAILURE);
    }

    //Get options
    config_t config = {0};
    if (!set_config_setting(&config, argc, argv)){
        exit(EXIT_FAILURE);
    }

    //Init std
    sdl_t sdl = {0};
    if (!init_sdl(&sdl, config)){
        exit(EXIT_FAILURE);
    }

    //Screen Clear
    clear_window(config, sdl);

    //main emulator loop
    while(chip8.state != QUIT){
        //get input from the user
        get_input(&chip8);

        if (chip8.state == PAUSED) continue;

        emulate_instruction(&chip8, &config);

        //delay for 60hz (chip8 standard)
        SDL_Delay(16);
        update_window(sdl, config, chip8);

    }


    //Shutdown SDL
    final_closedown(&sdl);

    exit(EXIT_SUCCESS);
}
