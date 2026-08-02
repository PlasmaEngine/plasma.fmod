#pragma once

#include <fmod_studio.hpp>

#define PL_FMOD_ASSERT(func)                                                                         \
  {                                                                                                  \
    auto fmodErrorCode = func;                                                                       \
    if (fmodErrorCode != FMOD_OK)                                                                    \
      plLog::Error("FMOD call failed: '" PL_PP_STRINGIFY(func) "' - Error code {0}", fmodErrorCode); \
  }
