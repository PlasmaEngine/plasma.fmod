#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodSoundBankResource, 1, plRTTIDefaultAllocator<plFmodSoundBankResource>)
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_RESOURCE_IMPLEMENT_COMMON_CODE(plFmodSoundBankResource);

plFmodSoundBankResource::plFmodSoundBankResource()
  : plResource(DoUpdate::OnAnyThread, 1)
{
  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(plFmodSoundBankResource);
}

plFmodSoundBankResource::~plFmodSoundBankResource()
{
  PL_ASSERT_DEV(m_pSoundBank == nullptr, "Soundbank has not been freed correctly");
}

plResourceLoadDesc plFmodSoundBankResource::UnloadData(Unload WhatToUnload)
{
  if (m_pSoundBank)
  {
    m_pSoundBank->unload();
    m_pSoundBank = nullptr;
  }

  plFmod::GetSingleton()->QueueSoundBankDataForDeletion(m_pSoundBankData);
  m_pSoundBankData = nullptr;

  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(plFmodSoundBankResource);

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = plResourceState::Unloaded;

  return res;
}

plResourceLoadDesc plFmodSoundBankResource::UpdateContent(plStreamReader* Stream)
{
  PL_LOG_BLOCK("plFmodSoundBankResource::UpdateContent", GetResourceIdOrDescription());

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;

  if (Stream == nullptr)
  {
    res.m_State = plResourceState::LoadedResourceMissing;
    return res;
  }

  Stream->ReadBytes(&m_pSoundBank, sizeof(FMOD::Studio::Bank*));
  Stream->ReadBytes(&m_pSoundBankData, sizeof(plDataBuffer*));

  PL_ASSERT_DEV(m_pSoundBank != nullptr, "Invalid Sound Bank pointer in stream");
  PL_ASSERT_DEV(m_pSoundBankData != nullptr, "Invalid Sound Bank Data pointer in stream");

  res.m_State = plResourceState::Loaded;

  // the newly loaded sound bank might contain VCAs that had not been loaded yet
  plFmod::GetSingleton()->UpdateSoundGroupVolumes();

  plFmod::GetSingleton()->RegisterTriggerMappingsForBank(GetResourceID(), m_pSoundBank);

  return res;
}

void plFmodSoundBankResource::UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage)
{
  out_NewMemoryUsage.m_uiMemoryCPU = sizeof(plFmodSoundBankResource);

  if (m_pSoundBankData)
  {
    out_NewMemoryUsage.m_uiMemoryCPU += (plUInt32)m_pSoundBankData->GetHeapMemoryUsage() + sizeof(*m_pSoundBankData);
  }

  out_NewMemoryUsage.m_uiMemoryGPU = 0;
}

PL_RESOURCE_IMPLEMENT_CREATEABLE(plFmodSoundBankResource, plFmodSoundBankResourceDescriptor)
{
  // have to create one 'missing' resource
  // PL_REPORT_FAILURE("This resource type does not support creating data.");

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = plResourceState::Loaded;

  return res;
}



PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_Resources_FmodSoundBankResource);
