#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodSoundEventResource, 1, plRTTIDefaultAllocator<plFmodSoundEventResource>)
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_RESOURCE_IMPLEMENT_COMMON_CODE(plFmodSoundEventResource);

plFmodSoundEventResource::plFmodSoundEventResource()
  : plResource(DoUpdate::OnAnyThread, 1)
{
  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(plFmodSoundEventResource);
}

plFmodSoundEventResource::~plFmodSoundEventResource()
{
  PL_ASSERT_DEV(m_pEventDescription == nullptr, "SoundEvent has not been freed correctly");
}

plResult plFmodSoundEventResource::PlayOnce(const plTransform& globalPosition, float fPitch /*= 1.0f*/, float fVolume /*= 1.0f*/) const
{
  bool bIsOneShot = false;
  m_pEventDescription->isOneshot(&bIsOneShot);

  if (!bIsOneShot)
  {
    plLog::Warning("plFmodSoundEventResource::PlayOnce: '{}' is not a one-shot event.", GetResourceIdOrDescription());
    return PL_FAILURE;
  }

  auto pInstance = CreateInstance();
  if (pInstance == nullptr)
  {
    plLog::Warning("plFmodSoundEventResource::PlayOnce: Instance of '{}' could not be created.", GetResourceIdOrDescription());
    return PL_FAILURE;
  }
  const auto fwd = globalPosition.m_qRotation * plVec3(1, 0, 0);
  const auto up = globalPosition.m_qRotation * plVec3(0, 0, 1);

  FMOD_3D_ATTRIBUTES attr;
  attr.position.x = globalPosition.m_vPosition.x;
  attr.position.y = globalPosition.m_vPosition.y;
  attr.position.z = globalPosition.m_vPosition.z;
  attr.forward.x = fwd.x;
  attr.forward.y = fwd.y;
  attr.forward.z = fwd.z;
  attr.up.x = up.x;
  attr.up.y = up.y;
  attr.up.z = up.z;
  attr.velocity.x = 0;
  attr.velocity.y = 0;
  attr.velocity.z = 0;

  PL_FMOD_ASSERT(pInstance->setPitch(fPitch));
  PL_FMOD_ASSERT(pInstance->setVolume(fVolume));
  PL_FMOD_ASSERT(pInstance->set3DAttributes(&attr));
  PL_FMOD_ASSERT(pInstance->setPaused(false));
  PL_FMOD_ASSERT(pInstance->start());

  pInstance->release();

  return PL_SUCCESS;
}

FMOD::Studio::EventInstance* plFmodSoundEventResource::CreateInstance() const
{
  if (m_pEventDescription)
  {
    FMOD::Studio::EventInstance* pInstance = nullptr;
    PL_FMOD_ASSERT(m_pEventDescription->createInstance(&pInstance));
    return pInstance;
  }

  return nullptr;
}

plResourceLoadDesc plFmodSoundEventResource::UnloadData(Unload WhatToUnload)
{
  if (m_pEventDescription)
  {
    // this will kill all event pointers in the components
    // which is why the components actually listen for unload events on these resources
    m_pEventDescription->releaseAllInstances();
    m_pEventDescription->unloadSampleData();
    m_pEventDescription = nullptr;
  }

  m_hSoundBank.Invalidate();

  ModifyMemoryUsage().m_uiMemoryCPU = sizeof(plFmodSoundEventResource);

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = plResourceState::Unloaded;

  return res;
}

plResourceLoadDesc plFmodSoundEventResource::UpdateContent(plStreamReader* Stream)
{
  PL_LOG_BLOCK("plFmodSoundEventResource::UpdateContent", GetResourceIdOrDescription());

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;

  if (Stream == nullptr)
  {
    res.m_State = plResourceState::LoadedResourceMissing;
    return res;
  }

  plFmodSoundBankResourceHandle* pBankHandle = nullptr;
  Stream->ReadBytes(&pBankHandle, sizeof(plFmodSoundBankResourceHandle*));
  PL_ASSERT_DEV(pBankHandle != nullptr, "Invalid Sound Bank Handle pointer in stream");

  Stream->ReadBytes(&m_pEventDescription, sizeof(FMOD::Studio::EventDescription*));
  PL_ASSERT_DEV(m_pEventDescription != nullptr, "Invalid Sound Event Descriptor pointer in stream");

  m_hSoundBank = *pBankHandle;

  res.m_State = plResourceState::Loaded;
  return res;
}

void plFmodSoundEventResource::UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage)
{
  // we cannot compute this data here, so we update it wherever we know the memory usage

  out_NewMemoryUsage.m_uiMemoryCPU = ModifyMemoryUsage().m_uiMemoryCPU;
  out_NewMemoryUsage.m_uiMemoryGPU = 0;
}

PL_RESOURCE_IMPLEMENT_CREATEABLE(plFmodSoundEventResource, plFmodSoundEventResourceDescriptor)
{
  // one missing resource is created this way
  // PL_REPORT_FAILURE("This resource type does not support creating data.");

  plResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;
  res.m_State = plResourceState::Loaded;

  return res;
}



PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_Resources_FmodSoundEventResource);
