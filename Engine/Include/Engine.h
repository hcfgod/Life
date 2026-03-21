#pragma once

// Entry point
#ifdef LIFE_ENABLE_ENTRYPOINT
#include "Core/EntryPoint.h"
#elif defined(LIFE_ENABLE_SDL_ENTRYPOINT)
#include "Core/SDLEntryPoint.h"
#endif

#include "Core/Application.h"
#include "Core/Events.h"
#include "Core/Log.h"
#include "Core/Memory.h"
#include "Core/Window.h"
