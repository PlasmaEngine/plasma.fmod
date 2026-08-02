#include <FmodPlugin/FmodPluginPCH.h>

#include <AudioSystem/Core/AudioSystem.h>
#include <AudioSystem/Core/AudioSystemData.h>
#include <Core/ResourceManager/ResourceManager.h>
#include <FmodPlugin/FmodAtlData.h>
#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodMiddleware.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>

#if PL_ENABLED(PL_COMPILE_FOR_DEBUG) || PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
#  include <RendererCore/Debug/DebugRenderer.h>
#  include <RendererCore/Pipeline/Declarations.h>
#endif

PL_IMPLEMENT_SINGLETON(plFmodMiddleware);

plFmodAudioEntityData::plFmodAudioEntityData(plAudioSystemDataID uiEntityId, bool bIsGlobal)
  : m_uiEntityId(uiEntityId)
  , m_bGlobalEntity(bIsGlobal)
{
}

plFmodAudioListenerData::plFmodAudioListenerData(plAudioSystemDataID uiListenerId)
  : m_uiListenerId(uiListenerId)
{
}

plFmodAudioEventData::plFmodAudioEventData(plAudioSystemDataID uiEventId)
  : m_uiEventId(uiEventId)
{
}

plFmodAudioTriggerData::plFmodAudioTriggerData(plAudioSystemDataID uiTriggerId, plStringView sResourceId)
  : m_uiTriggerId(uiTriggerId)
  , m_sResourceId(sResourceId)
{
}

plFmodAudioBankData::plFmodAudioBankData(plAudioSystemDataID uiBankId, plStringView sResourceId)
  : m_uiBankId(uiBankId)
  , m_sResourceId(sResourceId)
{
}

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioEntityData, 1, plRTTIDefaultAllocator<plFmodAudioEntityData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioListenerData, 1, plRTTIDefaultAllocator<plFmodAudioListenerData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioEventData, 1, plRTTIDefaultAllocator<plFmodAudioEventData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioTriggerData, 1, plRTTIDefaultAllocator<plFmodAudioTriggerData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioBankData, 1, plRTTIDefaultAllocator<plFmodAudioBankData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioRtpcData, 1, plRTTIDefaultAllocator<plFmodAudioRtpcData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioSwitchStateData, 1, plRTTIDefaultAllocator<plFmodAudioSwitchStateData>)
PL_END_DYNAMIC_REFLECTED_TYPE;
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAudioEnvironmentData, 1, plRTTIDefaultAllocator<plFmodAudioEnvironmentData>)
PL_END_DYNAMIC_REFLECTED_TYPE;

static void BuildFmodEventPath(plStringView sTriggerName, plStringBuilder& out_sEventPath)
{
  out_sEventPath = sTriggerName;

  if (out_sEventPath.StartsWith_NoCase("event:/"))
    return;

  if (out_sEventPath.StartsWith("/"))
  {
    out_sEventPath.Prepend("event:");
    return;
  }

  out_sEventPath.Prepend("event:/");
}

static void ApplyTransformToInstance(FMOD::Studio::EventInstance* pInstance, const plAudioSystemTransform& transform)
{
  const plVec3 vForward = transform.m_vForward.IsZero() ? plVec3(1.0f, 0.0f, 0.0f) : transform.m_vForward;
  const plVec3 vUp = transform.m_vUp.IsZero() ? plVec3(0.0f, 0.0f, 1.0f) : transform.m_vUp;

  FMOD_3D_ATTRIBUTES attr;
  attr.position.x = transform.m_vPosition.x;
  attr.position.y = transform.m_vPosition.y;
  attr.position.z = transform.m_vPosition.z;
  attr.forward.x = vForward.x;
  attr.forward.y = vForward.y;
  attr.forward.z = vForward.z;
  attr.up.x = vUp.x;
  attr.up.y = vUp.y;
  attr.up.z = vUp.z;
  attr.velocity.x = transform.m_vVelocity.x;
  attr.velocity.y = transform.m_vVelocity.y;
  attr.velocity.z = transform.m_vVelocity.z;

  pInstance->set3DAttributes(&attr);
}

static plResult StartEventInstance(
  FMOD::Studio::EventInstance* pInstance, plFmodAudioEventData* pFmodEventData, bool bLooping, plStringView sDebugName)
{
  if (pInstance == nullptr)
    return PL_FAILURE;

  // Always stop+release the previous instance tracked by this event data slot.
  // For one-shot events this guarantees cleanup even if FMOD's auto-release
  // doesn't reclaim them (e.g. due to long tails or middleware quirks).
  if (pFmodEventData != nullptr && pFmodEventData->m_pEventInstance != nullptr)
  {
    pFmodEventData->m_pEventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
    pFmodEventData->m_pEventInstance->release();
    pFmodEventData->m_pEventInstance = nullptr;
  }

  const FMOD_RESULT startResult = pInstance->start();
  if (startResult != FMOD_OK)
  {
    plLog::Error("FMOD middleware failed to start event '{0}'.", sDebugName);
    pInstance->release();
    return PL_FAILURE;
  }

#if PL_ENABLED(PL_COMPILE_FOR_DEBUG) || PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
  {
    bool bOneShot = false;
    FMOD::Studio::EventDescription* pDesc = nullptr;
    if (pInstance->getDescription(&pDesc) == FMOD_OK && pDesc != nullptr)
      pDesc->isOneshot(&bOneShot);
  }
#endif

  // Always keep a reference to the instance so it can be explicitly stopped
  // on re-trigger or component teardown. Relying solely on FMOD's auto-release
  // for fire-and-forget one-shots can leave orphaned instances that accumulate
  // if the event doesn't reach STOPPED state promptly (sustain points, long
  // tails, or middleware timing). Explicit management caps active instances at
  // one per trigger event-data slot.
  if (pFmodEventData != nullptr)
  {
    pFmodEventData->m_pEventInstance = pInstance;
  }
  else
  {
    // No event data to track this instance — release so FMOD can try auto-cleanup.
    pInstance->release();
  }

  return PL_SUCCESS;
}

static plResult PlayFmodEventPath(plFmod& backend, plStringView sTriggerName, const plAudioSystemTransform& transform, plFmodAudioEventData* pFmodEventData, bool bLooping)
{
  FMOD::Studio::System* pStudioSystem = backend.GetStudioSystem();
  if (pStudioSystem == nullptr)
    return PL_FAILURE;

  plStringBuilder sEventPath;
  BuildFmodEventPath(sTriggerName, sEventPath);

  FMOD::Studio::EventDescription* pEventDescription = nullptr;
  if (pStudioSystem->getEvent(sEventPath.GetData(), &pEventDescription) != FMOD_OK || pEventDescription == nullptr)
  {
    plLog::Error("FMOD middleware failed to find event '{0}' for AudioSystem trigger.", sEventPath);
    return PL_FAILURE;
  }

  FMOD::Studio::EventInstance* pInstance = nullptr;
  if (pEventDescription->createInstance(&pInstance) != FMOD_OK || pInstance == nullptr)
  {
    plLog::Error("FMOD middleware failed to create event instance for '{0}'.", sEventPath);
    return PL_FAILURE;
  }

  ApplyTransformToInstance(pInstance, transform);
  return StartEventInstance(pInstance, pFmodEventData, bLooping, sEventPath);
}

static plResult PlayFmodResourceEvent(
  plStringView sResourceId, const plAudioSystemTransform& transform, plFmodAudioEventData* pFmodEventData, bool bLooping)
{
  plFmodSoundEventResourceHandle hSound;
  if (pFmodEventData != nullptr && pFmodEventData->m_hSoundEventResource.IsValid())
  {
    hSound = pFmodEventData->m_hSoundEventResource;
  }
  else
  {
    hSound = plResourceManager::LoadResource<plFmodSoundEventResource>(sResourceId);
    if (pFmodEventData != nullptr)
    {
      pFmodEventData->m_hSoundEventResource = hSound;
    }
  }

  if (!hSound.IsValid())
    return PL_FAILURE;

  plResourceLock<plFmodSoundEventResource> pSound(hSound, plResourceAcquireMode::AllowLoadingFallback_NeverFail);
  if (pSound.GetAcquireResult() != plResourceAcquireResult::Final)
    return PL_FAILURE;

  FMOD::Studio::EventInstance* pInstance = pSound->CreateInstance();
  if (pInstance == nullptr)
    return PL_FAILURE;

  ApplyTransformToInstance(pInstance, transform);
  return StartEventInstance(pInstance, pFmodEventData, bLooping, sResourceId);
}

static bool ResolveTriggerToResourceId(const plFmod& backend, const plAudioSystemTriggerData* pTriggerData, plStringBuilder& out_sResourceId)
{
  if (const auto* pTrigger = plDynamicCast<const plFmodAudioTriggerData*>(pTriggerData))
  {
    if (pTrigger->m_sResourceId.IsEmpty())
      return false;

    out_sResourceId = pTrigger->m_sResourceId;
    return out_sResourceId.FindSubString("|") != nullptr;
  }

  const auto* pStringTrigger = plDynamicCast<const plAudioSystemStringTriggerData*>(pTriggerData);
  if (pStringTrigger == nullptr || pStringTrigger->m_sTriggerName.IsEmpty())
    return false;

  if (!backend.TryResolveLooseTriggerToResourceId(pStringTrigger->m_sTriggerName, out_sResourceId))
    return false;

  return out_sResourceId.FindSubString("|") != nullptr;
}

plFmodMiddleware::plFmodMiddleware()
  : m_SingletonRegistrar(this)
{
}

plFmodMiddleware::~plFmodMiddleware() = default;

plResult plFmodMiddleware::LoadConfiguration(const plOpenDdlReaderElement& reader)
{
  PL_IGNORE_UNUSED(reader);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::Startup()
{
  m_pBackend = plFmod::GetSingleton();

  if (m_pBackend == nullptr)
    return PL_FAILURE;

  m_pBackend->Startup();
  return m_pBackend->GetStudioSystem() != nullptr ? PL_SUCCESS : PL_FAILURE;
}

void plFmodMiddleware::Update(plTime delta)
{
  PL_IGNORE_UNUSED(delta);

  if (m_pBackend != nullptr)
  {
    m_pBackend->UpdateSound();
  }
}

plResult plFmodMiddleware::Shutdown()
{
  for (auto it = m_LoadedStringBanks.GetIterator(); it.IsValid(); ++it)
  {
    it.Value().Invalidate();
  }
  m_LoadedStringBanks.Clear();

  if (m_pBackend != nullptr)
  {
    m_pBackend->Shutdown();
  }

  return PL_SUCCESS;
}

plResult plFmodMiddleware::Release()
{
  return PL_SUCCESS;
}

plResult plFmodMiddleware::StopAllSounds()
{
  if (m_pBackend == nullptr || m_pBackend->GetLowLevelSystem() == nullptr)
    return PL_FAILURE;

  FMOD::ChannelGroup* pChannel = nullptr;
  if (m_pBackend->GetLowLevelSystem()->getMasterChannelGroup(&pChannel) == FMOD_OK && pChannel != nullptr)
  {
    pChannel->stop();
    return PL_SUCCESS;
  }

  return PL_FAILURE;
}

plResult plFmodMiddleware::AddEntity(plAudioSystemEntityData* pEntityData, const char* szEntityName)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(szEntityName);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::ResetEntity(plAudioSystemEntityData* pEntityData)
{
  PL_IGNORE_UNUSED(pEntityData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::UpdateEntity(plAudioSystemEntityData* pEntityData)
{
  PL_IGNORE_UNUSED(pEntityData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::RemoveEntity(plAudioSystemEntityData* pEntityData)
{
  PL_IGNORE_UNUSED(pEntityData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetEntityTransform(plAudioSystemEntityData* pEntityData, const plAudioSystemTransform& Transform)
{
  auto* pFmodEntity = plDynamicCast<plFmodAudioEntityData*>(pEntityData);
  if (pFmodEntity == nullptr)
    return PL_FAILURE;

  pFmodEntity->m_LastTransform = Transform;
  return PL_SUCCESS;
}

plResult plFmodMiddleware::LoadTrigger(plAudioSystemEntityData* pEntityData, const plAudioSystemTriggerData* pTriggerData, plAudioSystemEventData* pEventData)
{
  PL_IGNORE_UNUSED(pEntityData);
  auto* pFmodEventData = plDynamicCast<plFmodAudioEventData*>(pEventData);
  if (pFmodEventData == nullptr || m_pBackend == nullptr)
    return PL_FAILURE;

  plStringBuilder sResourceId;
  if (!ResolveTriggerToResourceId(*m_pBackend, pTriggerData, sResourceId))
  {
    // Resolve expensive fallback during trigger-load (typically init), not on playback hot path.
    const auto* pStringTrigger = plDynamicCast<const plAudioSystemStringTriggerData*>(pTriggerData);
    if (pStringTrigger == nullptr || !m_pBackend->TryInferResourceIdFromLooseTrigger(pStringTrigger->m_sTriggerName, sResourceId))
      return PL_SUCCESS;
  }

  pFmodEventData->m_hSoundEventResource = plResourceManager::LoadResource<plFmodSoundEventResource>(sResourceId);
  return pFmodEventData->m_hSoundEventResource.IsValid() ? PL_SUCCESS : PL_FAILURE;
}

plResult plFmodMiddleware::ActivateTrigger(plAudioSystemEntityData* pEntityData, const plAudioSystemTriggerData* pTriggerData, plAudioSystemEventData* pEventData)
{
  if (m_pBackend == nullptr)
    return PL_FAILURE;

  plTransform t = plTransform::MakeIdentity();
  plAudioSystemTransform entityTransform;

  if (const auto* pEntity = plDynamicCast<const plFmodAudioEntityData*>(pEntityData))
  {
    entityTransform = pEntity->m_LastTransform;
    t.m_vPosition = entityTransform.m_vPosition;
  }

  if (const auto* pTrigger = plDynamicCast<const plFmodAudioTriggerData*>(pTriggerData))
  {
    if (pTrigger->m_sResourceId.IsEmpty())
      return PL_FAILURE;

    return PlayFmodResourceEvent(pTrigger->m_sResourceId, entityTransform, plDynamicCast<plFmodAudioEventData*>(pEventData), pEventData != nullptr && pEventData->m_bLooping);
  }

  const auto* pStringTrigger = plDynamicCast<const plAudioSystemStringTriggerData*>(pTriggerData);
  if (pStringTrigger == nullptr || pStringTrigger->m_sTriggerName.IsEmpty())
    return PL_FAILURE;

  // Prefer precomputed trigger map aliases loaded at startup.
  plStringBuilder sResolvedTrigger;
  if (m_pBackend->TryResolveLooseTriggerToResourceId(pStringTrigger->m_sTriggerName, sResolvedTrigger))
  {
    if (sResolvedTrigger.FindSubString("|") != nullptr)
    {
      return PlayFmodResourceEvent(sResolvedTrigger, entityTransform, plDynamicCast<plFmodAudioEventData*>(pEventData), pEventData != nullptr && pEventData->m_bLooping);
    }

    return PlayFmodEventPath(*m_pBackend, sResolvedTrigger, entityTransform, plDynamicCast<plFmodAudioEventData*>(pEventData), pEventData != nullptr && pEventData->m_bLooping);
  }

  plLog::Warning("FMOD trigger lookup miss for '{0}'. Readable aliases: {1}, hash mappings: {2}. Falling back to direct event path lookup.",
    pStringTrigger->m_sTriggerName, m_pBackend->GetReadableTriggerAliasCount(), m_pBackend->GetTriggerMappingCount());
  return PlayFmodEventPath(*m_pBackend, pStringTrigger->m_sTriggerName, entityTransform, plDynamicCast<plFmodAudioEventData*>(pEventData), pEventData != nullptr && pEventData->m_bLooping);
}

plResult plFmodMiddleware::UnloadTrigger(plAudioSystemEntityData* pEntityData, const plAudioSystemTriggerData* pTriggerData)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(pTriggerData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::StopEvent(plAudioSystemEntityData* pEntityData, const plAudioSystemEventData* pEventData)
{
  PL_IGNORE_UNUSED(pEntityData);

  auto* pFmodEventData = const_cast<plFmodAudioEventData*>(plDynamicCast<const plFmodAudioEventData*>(pEventData));
  if (pFmodEventData == nullptr)
    return PL_FAILURE;

  if (pFmodEventData->m_pEventInstance == nullptr)
    return PL_SUCCESS;

  pFmodEventData->m_pEventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT);
  pFmodEventData->m_pEventInstance->release();
  pFmodEventData->m_pEventInstance = nullptr;
  return PL_SUCCESS;
}

plResult plFmodMiddleware::StopAllEvents(plAudioSystemEntityData* pEntityData)
{
  PL_IGNORE_UNUSED(pEntityData);
  return StopAllSounds();
}

bool plFmodMiddleware::IsEventDataPlaying(const plAudioSystemEventData* pEventData) const
{
  auto* pFmodEventData = const_cast<plFmodAudioEventData*>(plDynamicCast<const plFmodAudioEventData*>(pEventData));
  if (pFmodEventData == nullptr || pFmodEventData->m_pEventInstance == nullptr)
    return false;

  FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
  if (pFmodEventData->m_pEventInstance->getPlaybackState(&state) != FMOD_OK)
  {
    pFmodEventData->m_pEventInstance = nullptr;
    return false;
  }

  if (state == FMOD_STUDIO_PLAYBACK_PLAYING || state == FMOD_STUDIO_PLAYBACK_STARTING || state == FMOD_STUDIO_PLAYBACK_SUSTAINING)
    return true;

  // Explicitly release stale instances when playback has finished.
  pFmodEventData->m_pEventInstance->release();
  pFmodEventData->m_pEventInstance = nullptr;
  return false;
}

plResult plFmodMiddleware::SetRtpc(plAudioSystemEntityData* pEntityData, const plAudioSystemRtpcData* pRtpcData, float fValue)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(pRtpcData);
  PL_IGNORE_UNUSED(fValue);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::ResetRtpc(plAudioSystemEntityData* pEntityData, const plAudioSystemRtpcData* pRtpcData)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(pRtpcData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetSwitchState(plAudioSystemEntityData* pEntityData, const plAudioSystemSwitchStateData* pSwitchStateData)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(pSwitchStateData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetObstructionAndOcclusion(plAudioSystemEntityData* pEntityData, float fObstruction, float fOcclusion)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(fObstruction);
  PL_IGNORE_UNUSED(fOcclusion);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetEnvironmentAmount(plAudioSystemEntityData* pEntityData, const plAudioSystemEnvironmentData* pEnvironmentData, float fAmount)
{
  PL_IGNORE_UNUSED(pEntityData);
  PL_IGNORE_UNUSED(pEnvironmentData);
  PL_IGNORE_UNUSED(fAmount);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::AddListener(plAudioSystemListenerData* pListenerData, const char* szListenerName, bool bIsDefault)
{
  PL_IGNORE_UNUSED(szListenerName);
  PL_IGNORE_UNUSED(bIsDefault);

  const auto* pListener = plDynamicCast<const plFmodAudioListenerData*>(pListenerData);
  if (pListener == nullptr)
    return PL_FAILURE;

  m_pBackend->SetNumListeners(plMath::Max<plUInt8>(m_pBackend->GetNumListeners(), static_cast<plUInt8>(pListener->m_uiListenerId + 1)));
  return PL_SUCCESS;
}

plResult plFmodMiddleware::ResetListener(plAudioSystemListenerData* pListenerData)
{
  PL_IGNORE_UNUSED(pListenerData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::RemoveListener(plAudioSystemListenerData* pListenerData)
{
  PL_IGNORE_UNUSED(pListenerData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetListenerTransform(plAudioSystemListenerData* pListenerData, const plAudioSystemTransform& Transform)
{
  const auto* pListener = plDynamicCast<const plFmodAudioListenerData*>(pListenerData);
  if (pListener == nullptr || m_pBackend == nullptr)
    return PL_FAILURE;

  m_pBackend->SetListener(static_cast<plInt32>(pListener->m_uiListenerId), Transform.m_vPosition, Transform.m_vForward, Transform.m_vUp, Transform.m_vVelocity);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::LoadBank(plAudioSystemBankData* pBankData)
{
  if (auto* pBank = plDynamicCast<plFmodAudioBankData*>(pBankData))
  {
    if (pBank->m_sResourceId.IsEmpty())
      return PL_FAILURE;

    pBank->m_hResource = plResourceManager::LoadResource<plFmodSoundBankResource>(pBank->m_sResourceId);
    if (!pBank->m_hResource.IsValid())
      return PL_FAILURE;

    plResourceLock<plFmodSoundBankResource> pLoadedBank(pBank->m_hResource, plResourceAcquireMode::BlockTillLoaded);
    return pLoadedBank.GetAcquireResult() == plResourceAcquireResult::Final ? PL_SUCCESS : PL_FAILURE;
  }

  auto* pStringBank = plDynamicCast<plAudioSystemStringBankData*>(pBankData);
  if (pStringBank == nullptr || pStringBank->m_sBankName.IsEmpty())
    return PL_FAILURE;

  plFmodSoundBankResourceHandle hBank = plResourceManager::LoadResource<plFmodSoundBankResource>(pStringBank->m_sBankName);
  if (!hBank.IsValid())
    return PL_FAILURE;

  plResourceLock<plFmodSoundBankResource> pLoadedBank(hBank, plResourceAcquireMode::BlockTillLoaded);
  if (pLoadedBank.GetAcquireResult() != plResourceAcquireResult::Final)
    return PL_FAILURE;

  m_LoadedStringBanks[pStringBank->m_uiBankId] = hBank;
  return PL_SUCCESS;
}

plResult plFmodMiddleware::UnloadBank(plAudioSystemBankData* pBankData)
{
  if (auto* pBank = plDynamicCast<plFmodAudioBankData*>(pBankData))
  {
    pBank->m_hResource.Invalidate();
    return PL_SUCCESS;
  }

  auto* pStringBank = plDynamicCast<plAudioSystemStringBankData*>(pBankData);
  if (pStringBank == nullptr)
    return PL_FAILURE;

  m_LoadedStringBanks.Remove(pStringBank->m_uiBankId);
  return PL_SUCCESS;
}

plAudioSystemEntityData* plFmodMiddleware::CreateWorldEntity(plAudioSystemDataID uiEntityId)
{
  return PL_AUDIOSYSTEM_NEW(plFmodAudioEntityData, uiEntityId, true);
}

plAudioSystemEntityData* plFmodMiddleware::CreateEntityData(plAudioSystemDataID uiEntityId)
{
  return PL_AUDIOSYSTEM_NEW(plFmodAudioEntityData, uiEntityId, false);
}

plResult plFmodMiddleware::DestroyEntityData(plAudioSystemEntityData* pEntityData)
{
  PL_AUDIOSYSTEM_DELETE(pEntityData);
  return PL_SUCCESS;
}

plAudioSystemListenerData* plFmodMiddleware::CreateListenerData(plAudioSystemDataID uiListenerId)
{
  return PL_AUDIOSYSTEM_NEW(plFmodAudioListenerData, uiListenerId);
}

plResult plFmodMiddleware::DestroyListenerData(plAudioSystemListenerData* pListenerData)
{
  PL_AUDIOSYSTEM_DELETE(pListenerData);
  return PL_SUCCESS;
}

plAudioSystemEventData* plFmodMiddleware::CreateEventData(plAudioSystemDataID uiEventId)
{
  return PL_AUDIOSYSTEM_NEW(plFmodAudioEventData, uiEventId);
}

plResult plFmodMiddleware::ResetEventData(plAudioSystemEventData* pEventData)
{
  plResult res = StopEvent(nullptr, pEventData);
  if (auto* pFmodEventData = plDynamicCast<plFmodAudioEventData*>(pEventData))
  {
    pFmodEventData->m_hSoundEventResource.Invalidate();
  }
  return res;
}

plResult plFmodMiddleware::DestroyEventData(plAudioSystemEventData* pEventData)
{
  StopEvent(nullptr, pEventData).IgnoreResult();
  PL_AUDIOSYSTEM_DELETE(pEventData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::DestroyBank(plAudioSystemBankData* pBankData)
{
  PL_AUDIOSYSTEM_DELETE(pBankData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::DestroyTriggerData(plAudioSystemTriggerData* pTriggerData)
{
  PL_AUDIOSYSTEM_DELETE(pTriggerData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::DestroyRtpcData(plAudioSystemRtpcData* pRtpcData)
{
  PL_AUDIOSYSTEM_DELETE(pRtpcData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::DestroySwitchStateData(plAudioSystemSwitchStateData* pSwitchStateData)
{
  PL_AUDIOSYSTEM_DELETE(pSwitchStateData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::DestroyEnvironmentData(plAudioSystemEnvironmentData* pEnvironmentData)
{
  PL_AUDIOSYSTEM_DELETE(pEnvironmentData);
  return PL_SUCCESS;
}

plResult plFmodMiddleware::SetLanguage(const char* szLanguage)
{
  PL_IGNORE_UNUSED(szLanguage);
  return PL_SUCCESS;
}

const char* plFmodMiddleware::GetMiddlewareName() const
{
  return "FMOD";
}

float plFmodMiddleware::GetMasterGain() const
{
  return m_pBackend != nullptr ? m_pBackend->GetMasterChannelVolume() : 1.0f;
}

bool plFmodMiddleware::GetMute() const
{
  return m_pBackend != nullptr ? m_pBackend->GetMasterChannelMute() : false;
}

void plFmodMiddleware::OnMasterGainChange(float fGain)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetMasterChannelVolume(fGain);
  }
}

void plFmodMiddleware::OnMuteChange(bool bMute)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetMasterChannelMute(bMute);
  }
}

void plFmodMiddleware::OnMasterPauseChange(bool bPaused)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetMasterChannelPaused(bPaused);
  }
}

bool plFmodMiddleware::GetMasterPaused() const
{
  return m_pBackend != nullptr ? m_pBackend->GetMasterChannelPaused() : false;
}

void plFmodMiddleware::SetSoundGroupVolume(plStringView sGroupGuid, float fVolume)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetSoundGroupVolume(sGroupGuid, fVolume);
  }
}

float plFmodMiddleware::GetSoundGroupVolume(plStringView sGroupGuid) const
{
  return m_pBackend != nullptr ? m_pBackend->GetSoundGroupVolume(sGroupGuid) : 1.0f;
}

void plFmodMiddleware::SetNumListeners(plUInt8 uiNumListeners)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetNumListeners(uiNumListeners);
  }
}

plUInt8 plFmodMiddleware::GetNumListeners() const
{
  return m_pBackend != nullptr ? m_pBackend->GetNumListeners() : 0;
}

void plFmodMiddleware::SetGlobalParameter(plStringView sName, float fValue)
{
  if (m_pBackend != nullptr)
  {
    m_pBackend->SetGlobalParameter(sName, fValue);
  }
}

float plFmodMiddleware::GetGlobalParameter(plStringView sName) const
{
  return m_pBackend != nullptr ? m_pBackend->GetGlobalParameter(sName) : 0.0f;
}

void plFmodMiddleware::OnLoseFocus()
{
  OnMasterPauseChange(true);
}

void plFmodMiddleware::OnGainFocus()
{
  OnMasterPauseChange(false);
}

#if PL_ENABLED(PL_COMPILE_FOR_DEBUG) || PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
namespace
{
  const char* FmodPlaybackStateString(FMOD_STUDIO_PLAYBACK_STATE state)
  {
    switch (state)
    {
    case FMOD_STUDIO_PLAYBACK_PLAYING:
      return "playing";
    case FMOD_STUDIO_PLAYBACK_STARTING:
      return "starting";
    case FMOD_STUDIO_PLAYBACK_SUSTAINING:
      return "sustaining";
    default:
      return "?";
    }
  }
} // namespace

void plFmodMiddleware::DebugDraw(const plViewHandle& hView) const
{
  if (m_pBackend == nullptr)
    return;

  FMOD::Studio::System* pStudioSystem = m_pBackend->GetStudioSystem();
  if (pStudioSystem == nullptr)
    return;

  int iBankCount = 0;
  if (pStudioSystem->getBankCount(&iBankCount) != FMOD_OK || iBankCount <= 0)
  {
    plDebugRenderer::DrawInfoText(hView, plDebugTextPlacement::BottomRight, "AudioSystemPlaying", plFmt("--- Playing (FMOD): no banks ---"), plColor::LightGray);
    return;
  }

  plDynamicArray<FMOD::Studio::Bank*> banks;
  banks.SetCountUninitialized(iBankCount);
  int iActualBanks = 0;
  if (pStudioSystem->getBankList(banks.GetData(), iBankCount, &iActualBanks) != FMOD_OK)
    return;

  constexpr plUInt32 kMaxInstanceLines = 48;
  plUInt32 uiLinesDrawn = 0;

  plDebugRenderer::DrawInfoText(hView, plDebugTextPlacement::BottomRight, "AudioSystemPlaying", plFmt("--- Playing (FMOD) ---"), plColor::LightSkyBlue);

  char szPath[512];

  for (int b = 0; b < iActualBanks && uiLinesDrawn < kMaxInstanceLines; ++b)
  {
    FMOD::Studio::Bank* pBank = banks[b];
    if (pBank == nullptr)
      continue;

    int iEventCount = 0;
    if (pBank->getEventCount(&iEventCount) != FMOD_OK || iEventCount <= 0)
      continue;

    plDynamicArray<FMOD::Studio::EventDescription*> events;
    events.SetCountUninitialized(iEventCount);
    int iActualEvents = 0;
    if (pBank->getEventList(events.GetData(), iEventCount, &iActualEvents) != FMOD_OK)
      continue;

    for (int e = 0; e < iActualEvents && uiLinesDrawn < kMaxInstanceLines; ++e)
    {
      FMOD::Studio::EventDescription* pDesc = events[e];
      if (pDesc == nullptr)
        continue;

      int iInstCount = 0;
      if (pDesc->getInstanceCount(&iInstCount) != FMOD_OK || iInstCount <= 0)
        continue;

      plDynamicArray<FMOD::Studio::EventInstance*> instances;
      instances.SetCountUninitialized(iInstCount);
      int iActualInst = 0;
      if (pDesc->getInstanceList(instances.GetData(), iInstCount, &iActualInst) != FMOD_OK)
        continue;

      bool bOneshot = false;
      pDesc->isOneshot(&bOneshot);

      int iPathRetrieved = 0;
      plStringView sEventPath = "(unknown event)"_plsv;
      if (pDesc->getPath(szPath, PL_ARRAY_SIZE(szPath), &iPathRetrieved) == FMOD_OK && iPathRetrieved > 0)
        sEventPath = plStringView(szPath);

      const plColor headerColor = bOneshot ? plColor::DodgerBlue : plColor::Orange;

      int iPlaying = 0, iSustaining = 0, iStarting = 0;
      for (int i = 0; i < iActualInst; ++i)
      {
        FMOD::Studio::EventInstance* pInst = instances[i];
        if (pInst == nullptr)
          continue;

        FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_STOPPED;
        if (pInst->getPlaybackState(&state) != FMOD_OK)
          continue;

        if (state == FMOD_STUDIO_PLAYBACK_PLAYING)
          ++iPlaying;
        else if (state == FMOD_STUDIO_PLAYBACK_SUSTAINING)
          ++iSustaining;
        else if (state == FMOD_STUDIO_PLAYBACK_STARTING)
          ++iStarting;
      }

      const int iActiveCount = iPlaying + iSustaining + iStarting;
      if (iActiveCount <= 0)
        continue;

      plStringBuilder sSummary;
      sSummary.AppendFormat("{0}  x{1}", sEventPath, iActiveCount);
      if (!bOneshot)
        sSummary.Append("  [NOT oneshot]");
      if (iSustaining > 0)
        sSummary.AppendFormat("  ({0} sustaining)", iSustaining);

      plDebugRenderer::DrawInfoText(hView, plDebugTextPlacement::BottomRight, "AudioSystemPlaying",
        plFmt("{0}", sSummary), headerColor);
      ++uiLinesDrawn;
    }
  }

  if (uiLinesDrawn == 0)
  {
    plDebugRenderer::DrawInfoText(hView, plDebugTextPlacement::BottomRight, "AudioSystemPlaying", plFmt("(no active instances)"), plColor::LightGray);
  }
}
#endif

PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_FmodMiddleware);
