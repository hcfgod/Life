#pragma once

#include <math.h>
#include <stdlib.h>

#include <SDL3/SDL_cpuinfo.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>

#ifndef SDL_TARGETING
#define SDL_TARGETING(_target_)
#endif

#ifndef STBI_NO_FAILURE_STRINGS
static const char* stbi__g_failure_reason = "";
#endif

#include "stb_image_source.h"
