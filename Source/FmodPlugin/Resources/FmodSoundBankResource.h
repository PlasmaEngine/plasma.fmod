#pragma once

#include <Core/ResourceManager/Resource.h>
#include <FmodPlugin/FmodPluginDLL.h>

using plFmodSoundBankResourceHandle = plTypedResourceHandle<class plFmodSoundBankResource>;

struct PL_FMODPLUGIN_DLL plFmodSoundBankResourceDescriptor
{
  // empty, these types of resources must be loaded from file
};

class PL_FMODPLUGIN_DLL plFmodSoundBankResource : public plResource
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodSoundBankResource, plResource);
  PL_RESOURCE_DECLARE_COMMON_CODE(plFmodSoundBankResource);
  PL_RESOURCE_DECLARE_CREATEABLE(plFmodSoundBankResource, plFmodSoundBankResourceDescriptor);

public:
  plFmodSoundBankResource();
  ~plFmodSoundBankResource();

private:
  virtual plResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual plResourceLoadDesc UpdateContent(plStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

private:
  FMOD::Studio::Bank* m_pSoundBank = nullptr;
  plDataBuffer* m_pSoundBankData = nullptr;
};

class PL_FMODPLUGIN_DLL plFmodSoundBankResourceLoader : public plResourceTypeLoader
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
    FMOD::Studio::Bank* m_pSoundBank = nullptr;
    plDataBuffer* m_pSoundbankData = nullptr;
  };

  virtual plResourceLoadData OpenDataStream(const plResource* pResource) override;
  virtual void CloseDataStream(const plResource* pResource, const plResourceLoadData& loaderData) override;
  virtual bool IsResourceOutdated(const plResource* pResource) const override;
};
