#pragma once

#include <Core/ResourceManager/Resource.h>
#include <FmodPlugin/FmodPluginDLL.h>

using plFmodSoundEventResourceHandle = plTypedResourceHandle<class plFmodSoundEventResource>;
using plFmodSoundBankResourceHandle = plTypedResourceHandle<class plFmodSoundBankResource>;

struct PL_FMODPLUGIN_DLL plFmodSoundEventResourceDescriptor
{
  // empty, these types of resources must be loaded from file
};

class PL_FMODPLUGIN_DLL plFmodSoundEventResource : public plResource
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodSoundEventResource, plResource);
  PL_RESOURCE_DECLARE_COMMON_CODE(plFmodSoundEventResource);
  PL_RESOURCE_DECLARE_CREATEABLE(plFmodSoundEventResource, plFmodSoundEventResourceDescriptor);

public:
  plFmodSoundEventResource();
  ~plFmodSoundEventResource();

  /// \brief Creates an instance of this sound event and plays it.
  ///
  /// This is only allowed for events that are not looped, otherwise PL_FAILURE is returned.
  plResult PlayOnce(const plTransform& globalPosition, float fPitch = 1.0f, float fVolume = 1.0f) const;

  /// \brief Creates a new sound event instance of this FMOD sound event. May return nullptr, if the event data could not be loaded.
  FMOD::Studio::EventInstance* CreateInstance() const;

  /// \brief Returns the FMOD sound event descriptor. May be nullptr, if the sound bank could not be loaded or the event GUID was invalid.
  FMOD::Studio::EventDescription* GetDescriptor() const { return m_pEventDescription; }

private:
  virtual plResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual plResourceLoadDesc UpdateContent(plStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

private:
  plFmodSoundBankResourceHandle m_hSoundBank;
  FMOD::Studio::EventDescription* m_pEventDescription = nullptr;
};

class PL_FMODPLUGIN_DLL plFmodSoundEventResourceLoader : public plResourceTypeLoader
{
public:
  struct LoadedData
  {
    LoadedData()
      : m_Reader(&m_Storage)

    {
    }

    plDefaultMemoryStreamStorage m_Storage;
    plMemoryStreamReader m_Reader;
    plFmodSoundBankResourceHandle m_hSoundBank;
    FMOD::Studio::EventDescription* m_pEventDescription = nullptr;
  };

  virtual plResourceLoadData OpenDataStream(const plResource* pResource) override;
  virtual void CloseDataStream(const plResource* pResource, const plResourceLoadData& loaderData) override;
  virtual bool IsResourceOutdated(const plResource* pResource) const override;
};
