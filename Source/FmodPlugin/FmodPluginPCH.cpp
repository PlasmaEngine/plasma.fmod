#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/FmodPluginDLL.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Strings/TranslationLookup.h>

PL_STATICLINK_LIBRARY(FmodPlugin)
{
  if (bReturn)
    return;

  PL_STATICLINK_REFERENCE(FmodPlugin_Components_FmodComponent);
  PL_STATICLINK_REFERENCE(FmodPlugin_Components_FmodEventComponent);
  PL_STATICLINK_REFERENCE(FmodPlugin_Components_FmodListenerComponent);
  PL_STATICLINK_REFERENCE(FmodPlugin_FmodSingleton);
  PL_STATICLINK_REFERENCE(FmodPlugin_FmodStartup);
  PL_STATICLINK_REFERENCE(FmodPlugin_Resources_FmodSoundBankResource);
  PL_STATICLINK_REFERENCE(FmodPlugin_Resources_FmodSoundEventResource);
}
