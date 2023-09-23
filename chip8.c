#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include "SDL.h"

//https://github.com/kripod/chip8-roms

typedef struct{
    SDL_Window *window;
    SDL_Renderer *renderer;
}sdl_t;

typedef struct{
    uint32_t screenWidth;   //Chip8 width
    uint32_t screenHeight;  //Chip8 height
    uint32_t fgColor;       //RGBA 8888
    uint32_t bgColor;       //RGBA 8888
    uint32_t scaleFactor;   //Scale the size of a Chip8 pixel
}config_t;

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
}chip8_t;

//Set config settings
bool set_config_setting(config_t *config, const int argc, char **argv){
    //DEFAULTS
    config -> screenWidth = 64;     //Default chip8 width
    config -> screenHeight = 32;    //Default chip8 height

    config -> fgColor = 0xFFFFFFFF; //Color of pixels, chip8 was all white
    config -> bgColor = 0xFFF00FFF; //Color of background, Chip8 was all black
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

bool init_chip8(chip8_t *chip8){
    chip8 -> state = RUNNING;
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

void update_window(sdl_t sdl){
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

int main (int argc, char **argv){
    //set chip8 to the running state
    chip8_t chip8 = {0};
    if (!init_chip8(&chip8)){
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

        //delay for 60hz (chip8 standard)
        SDL_Delay(100);
        update_window(sdl);

    }


    //Shutdown SDL
    final_closedown(&sdl);

    exit(EXIT_SUCCESS);
}
