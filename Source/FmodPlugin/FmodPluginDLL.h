#pragma once

// Configure the DLL Import/Export Define
#if PL_ENABLED(PL_COMPILE_ENGINE_AS_DLL)
#  ifdef BUILDSYSTEM_BUILDING_FMODPLUGIN_LIB
#    define PL_FMODPLUGIN_DLL PL_DECL_EXPORT
#  elif defined(BUILDSYSTEM_FOLDING_PLUGIN_IMPORTS)
#    define PL_FMODPLUGIN_DLL
#  else
#    define PL_FMODPLUGIN_DLL PL_DECL_IMPORT
#  endif
#else
#  define PL_FMODPLUGIN_DLL
#endif

// Forward declarations

namespace FMOD
{
  namespace Studio
  {
    class Bank;
    class System;
    class EventInstance;
    class EventDescription;
  } // namespace Studio

  class System;
} // namespace FMOD
