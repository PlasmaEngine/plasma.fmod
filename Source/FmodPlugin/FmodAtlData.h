#pragma once

#include <AudioSystem/Core/AudioSystemData.h>
#include <Core/ResourceManager/ResourceHandle.h>
#include <FmodPlugin/FmodPluginDLL.h>

using plFmodSoundBankResourceHandle = plTypedResourceHandle<class plFmodSoundBankResource>;
using plFmodSoundEventResourceHandle = plTypedResourceHandle<class plFmodSoundEventResource>;

namespace FMOD
{
  namespace Studio
  {
    class EventInstance;
  }
}

class PL_FMODPLUGIN_DLL plFmodAudioEntityData final : public plAudioSystemEntityData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioEntityData, plAudioSystemEntityData);

public:
  plFmodAudioEntityData() = default;
  explicit plFmodAudioEntityData(plAudioSystemDataID uiEntityId, bool bIsGlobal = false);

  plAudioSystemDataID m_uiEntityId = 0;
  bool m_bGlobalEntity = false;
  plAudioSystemTransform m_LastTransform;
};

class PL_FMODPLUGIN_DLL plFmodAudioListenerData final : public plAudioSystemListenerData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioListenerData, plAudioSystemListenerData);

public:
  plFmodAudioListenerData() = default;
  explicit plFmodAudioListenerData(plAudioSystemDataID uiListenerId);

  plAudioSystemDataID m_uiListenerId = 0;
};

class PL_FMODPLUGIN_DLL plFmodAudioEventData final : public plAudioSystemEventData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioEventData, plAudioSystemEventData);

public:
  plFmodAudioEventData() = default;
  explicit plFmodAudioEventData(plAudioSystemDataID uiEventId);

  plAudioSystemDataID m_uiEventId = 0;
  FMOD::Studio::EventInstance* m_pEventInstance = nullptr;
  plFmodSoundEventResourceHandle m_hSoundEventResource;
};

class PL_FMODPLUGIN_DLL plFmodAudioTriggerData final : public plAudioSystemTriggerData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioTriggerData, plAudioSystemTriggerData);

public:
  plFmodAudioTriggerData() = default;
  explicit plFmodAudioTriggerData(plAudioSystemDataID uiTriggerId, plStringView sResourceId);

  plAudioSystemDataID m_uiTriggerId = 0;
  plString m_sResourceId;
};

class PL_FMODPLUGIN_DLL plFmodAudioBankData final : public plAudioSystemBankData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioBankData, plAudioSystemBankData);

public:
  plFmodAudioBankData() = default;
  explicit plFmodAudioBankData(plAudioSystemDataID uiBankId, plStringView sResourceId);

  plAudioSystemDataID m_uiBankId = 0;
  plString m_sResourceId;
  plFmodSoundBankResourceHandle m_hResource;
};

class PL_FMODPLUGIN_DLL plFmodAudioRtpcData final : public plAudioSystemRtpcData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioRtpcData, plAudioSystemRtpcData);
};

class PL_FMODPLUGIN_DLL plFmodAudioSwitchStateData final : public plAudioSystemSwitchStateData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioSwitchStateData, plAudioSystemSwitchStateData);
};

class PL_FMODPLUGIN_DLL plFmodAudioEnvironmentData final : public plAudioSystemEnvironmentData
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAudioEnvironmentData, plAudioSystemEnvironmentData);
};
