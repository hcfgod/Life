#pragma once

#include "Core/ApplicationRunner.h"

#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

int main(int argc, char** argv)
{
    SDL_SetMainReady();
    return Life::RunApplicationMain({ argc, argv });
}
