#include <FmodPlugin/FmodPluginPCH.h>

#include <Core/Interfaces/PhysicsWorldModule.h>
#include <Core/Messages/CommonMessages.h>
#include <Core/Messages/DeleteObjectMessage.h>
#include <Core/ResourceManager/Resource.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <FmodPlugin/Components/FmodEventComponent.h>
#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>
#include <Foundation/Configuration/CVar.h>
#include <RendererCore/Debug/DebugRenderer.h>
#include <RendererCore/Pipeline/View.h>
#include <RendererCore/RenderWorld/RenderWorld.h>

static_assert(sizeof(plFmodParameterId) == sizeof(FMOD_STUDIO_PARAMETER_ID));

PL_ALWAYS_INLINE FMOD_STUDIO_PARAMETER_ID ConvertPlToFmodId(plFmodParameterId paramId)
{
  return *reinterpret_cast<FMOD_STUDIO_PARAMETER_ID*>(&paramId);
}

PL_ALWAYS_INLINE plFmodParameterId ConvertFmodToPlId(FMOD_STUDIO_PARAMETER_ID paramId)
{
  return *reinterpret_cast<plFmodParameterId*>(&paramId);
}


//////////////////////////////////////////////////////////////////////////

// clang-format off
PL_IMPLEMENT_MESSAGE_TYPE(plMsgFmodSoundFinished);
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plMsgFmodSoundFinished, 1, plRTTIDefaultAllocator<plMsgFmodSoundFinished>)
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

//////////////////////////////////////////////////////////////////////////

plCVarInt cvar_FmodOcclusionNumRays("FMOD.Occlusion.NumRays", 2, plCVarFlags::Default, "Number of occlusion rays per component per frame");

static plVec3 s_InSpherePositions[32];
static bool s_bInSpherePositionsInitialized = false;

plFmodEventComponentManager::plFmodEventComponentManager(plWorld* pWorld)
  : plComponentManager(pWorld)
{
  if (!s_bInSpherePositionsInitialized)
  {
    s_bInSpherePositionsInitialized = true;

    plRandom rng;
    rng.Initialize(3);
    plRandomGauss rngGauss;
    rngGauss.Initialize(27, 0xFFFF);

    for (plUInt32 i = 0; i < PL_ARRAY_SIZE(s_InSpherePositions); ++i)
    {
      plVec3& pos = s_InSpherePositions[i];
      pos.x = (float)rngGauss.SignedValue();
      pos.y = (float)rngGauss.SignedValue();
      pos.z = (float)rngGauss.SignedValue();

      float fRadius = plMath::Pow((float)rng.DoubleZeroToOneExclusive(), 1.0f / 3.0f);
      fRadius = fRadius * 0.5f + 0.5f;
      pos.SetLength(fRadius).IgnoreResult();
    }
  }
}

void plFmodEventComponentManager::Initialize()
{
  SUPER::Initialize();

  {
    auto desc = PL_CREATE_MODULE_UPDATE_FUNCTION_DESC(plFmodEventComponentManager::UpdateOcclusion, this);
    desc.m_Phase = plWorldModule::UpdateFunctionDesc::Phase::Async;
    desc.m_bOnlyUpdateWhenSimulating = true;

    this->RegisterUpdateFunction(desc);
  }

  {
    auto desc = PL_CREATE_MODULE_UPDATE_FUNCTION_DESC(plFmodEventComponentManager::UpdateEvents, this);
    desc.m_Phase = plWorldModule::UpdateFunctionDesc::Phase::PostTransform;
    desc.m_bOnlyUpdateWhenSimulating = true;

    this->RegisterUpdateFunction(desc);
  }

  plResourceManager::GetResourceEvents().AddEventHandler(plMakeDelegate(&plFmodEventComponentManager::ResourceEventHandler, this));
}

void plFmodEventComponentManager::Deinitialize()
{
  plResourceManager::GetResourceEvents().RemoveEventHandler(plMakeDelegate(&plFmodEventComponentManager::ResourceEventHandler, this));

  SUPER::Deinitialize();
}

plUInt32 plFmodEventComponentManager::AddOcclusionState(plFmodEventComponent* pComponent, plFmodParameterId occlusionParamId, float fRadius)
{
  auto& occlusionState = m_OcclusionStates.ExpandAndGetRef();
  occlusionState.m_pComponent = pComponent;
  occlusionState.m_OcclusionParamId = occlusionParamId;
  occlusionState.m_fRadius = fRadius;

  if (const auto pPhysicsWorldModule = GetWorld()->GetModule<plPhysicsWorldModuleInterface>())
  {
    plVec3 listenerPos = plFmod::GetSingleton()->GetListenerPosition();
    ShootOcclusionRays(occlusionState, listenerPos, 8, pPhysicsWorldModule, plTime::MakeFromSeconds(1000.0));
  }

  return m_OcclusionStates.GetCount() - 1;
}

void plFmodEventComponentManager::RemoveOcclusionState(plUInt32 uiIndex)
{
  if (uiIndex >= m_OcclusionStates.GetCount())
    return;

  m_OcclusionStates.RemoveAtAndSwap(uiIndex);

  if (uiIndex != m_OcclusionStates.GetCount())
  {
    m_OcclusionStates[uiIndex].m_pComponent->m_uiOcclusionStateIndex = uiIndex;
  }
}

void plFmodEventComponentManager::ShootOcclusionRays(OcclusionState& state, plVec3 listenerPos, plUInt32 uiNumRays, const plPhysicsWorldModuleInterface* pPhysicsWorldModule, plTime deltaTime)
{
  uiNumRays = plMath::Min(uiNumRays, 32u);

  plVec3 centerPos = state.m_pComponent->GetOwner()->GetGlobalPosition();
  plUInt8 uiCollisionLayer = state.m_pComponent->m_uiOcclusionCollisionLayer;
  plPhysicsCastResult hitResult;

  for (plUInt32 i = 0; i < uiNumRays; ++i)
  {
    plUInt32 uiRayIndex = state.m_uiNextRayIndex;
    plVec3 targetPos = centerPos + s_InSpherePositions[uiRayIndex] * state.m_fRadius;
    plVec3 dir = targetPos - listenerPos;
    float fDistance = dir.GetLengthAndNormalize();

    plPhysicsQueryParameters query(uiCollisionLayer);
    query.m_bIgnoreInitialOverlap = true;
    query.m_ShapeTypes = plPhysicsShapeType::Static | plPhysicsShapeType::Dynamic;

    bool bHit = pPhysicsWorldModule->Raycast(hitResult, listenerPos, dir, fDistance, query);
    if (bHit)
    {
      state.m_uiRaycastHits |= (1 << uiRayIndex);
    }
    else
    {
      state.m_uiRaycastHits &= ~(1 << uiRayIndex);
    }

    state.m_uiNextRayIndex = (state.m_uiNextRayIndex + 1) % 32;
    state.m_uiNumUsedRays = plMath::Min<plUInt8>(state.m_uiNumUsedRays + 1, 32);
  }

  float fNewOcclusionValue = (float)plMath::CountBits(state.m_uiRaycastHits) / state.m_uiNumUsedRays;
  float fNormalizedDistance = plMath::Min((centerPos - listenerPos).GetLength() / state.m_fRadius, 1.0f);
  fNewOcclusionValue = plMath::Max(fNewOcclusionValue - 1.0f + fNormalizedDistance, 0.0f);
  state.m_fLastOcclusionValue = plMath::Lerp(state.m_fLastOcclusionValue, fNewOcclusionValue, plMath::Min(deltaTime.GetSeconds() * 8.0, 1.0));
}

void plFmodEventComponentManager::UpdateOcclusion(const plWorldModule::UpdateContext& context)
{
  if (const auto pPhysicsWorldModule = GetWorld()->GetModuleReadOnly<plPhysicsWorldModuleInterface>())
  {
    plVec3 listenerPos = plFmod::GetSingleton()->GetListenerPosition();
    plTime deltaTime = GetWorld()->GetClock().GetTimeDiff();

    plUInt32 uiNumRays = plMath::Clamp<int>(cvar_FmodOcclusionNumRays, 1, 32);

    for (auto& occlusionState : m_OcclusionStates)
    {
      ShootOcclusionRays(occlusionState, listenerPos, uiNumRays, pPhysicsWorldModule, deltaTime);
    }
  }
}

void plFmodEventComponentManager::UpdateEvents(const plWorldModule::UpdateContext& context)
{
  constexpr plUInt32 uiUpdatesPerSec = 20;
  constexpr plTime tUpdateRate = plTime::Milliseconds(1000 / uiUpdatesPerSec);

  const float fUpdateFraction = (GetWorld()->GetClock().GetTimeDiff() / tUpdateRate).AsFloatInSeconds();

  const plUInt32 uiNumComps = m_ComponentStorage.GetCount();

  if (m_uiFirstComponentIndex >= uiNumComps)
  {
    m_uiFirstComponentIndex = 0;
  }

  const plUInt32 uiNumUpdate = static_cast<plUInt32>(uiNumComps * fUpdateFraction) + 1;
  const plUInt32 uiLastCompP1 = plMath::Min(m_uiFirstComponentIndex + uiNumUpdate, uiNumComps);

  for (auto it = m_ComponentStorage.GetIterator(m_uiFirstComponentIndex, uiNumComps); it.IsValid(); ++it)
  {
    ComponentType* pComponent = it;

    // a lot of components will actually be inactive (waiting to be reused)
    if (pComponent->IsActiveAndInitialized())
    {
      pComponent->Update();
    }
  }

  m_uiFirstComponentIndex = uiLastCompP1;
}

void plFmodEventComponentManager::ResourceEventHandler(const plResourceEvent& e)
{
  if (e.m_Type == plResourceEvent::Type::ResourceContentUnloading && e.m_pResource->GetDynamicRTTI()->IsDerivedFrom<plFmodSoundEventResource>())
  {
    plFmodSoundEventResourceHandle hResource((plFmodSoundEventResource*)(e.m_pResource));

    for (auto it = GetComponents(); it.IsValid(); it.Next())
    {
      if (it->m_hSoundEvent == hResource)
      {
        it->InvalidateResource(true);
      }
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plFmodEventComponent, 4, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_ACCESSOR_PROPERTY("Paused", GetPaused, SetPaused),
    PL_ACCESSOR_PROPERTY("Volume", GetVolume, SetVolume)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_ACCESSOR_PROPERTY("Pitch", GetPitch, SetPitch)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.1f, 10.0f)),
    PL_ACCESSOR_PROPERTY("NoGlobalPitch", GetNoGlobalPitch, SetNoGlobalPitch),
    PL_RESOURCE_ACCESSOR_PROPERTY("SoundEvent", GetSoundEvent, SetSoundEvent)->AddAttributes(new plAssetBrowserAttribute("CompatibleAsset_Fmod_Event", plDependencyFlags::Package)),
    PL_ACCESSOR_PROPERTY("UseOcclusion", GetUseOcclusion, SetUseOcclusion),
    PL_ACCESSOR_PROPERTY("OcclusionThreshold", GetOcclusionThreshold, SetOcclusionThreshold)->AddAttributes(new plDefaultValueAttribute(0.5f), new plClampValueAttribute(0.0f, 1.0f)),
    PL_ACCESSOR_PROPERTY("OcclusionCollisionLayer", GetOcclusionCollisionLayer, SetOcclusionCollisionLayer)->AddAttributes(new plDynamicEnumAttribute("PhysicsCollisionLayer")),
    PL_ENUM_MEMBER_PROPERTY("OnFinishedAction", plOnComponentFinishedAction, m_OnFinishedAction),
    PL_ACCESSOR_PROPERTY("ShowDebugInfo", GetShowDebugInfo, SetShowDebugInfo),
    //PL_FUNCTION_PROPERTY("Preview", StartOneShot), // This doesn't seem to be working anymore, and I cannot find code for exposing it in the UI either
  }
  PL_END_PROPERTIES;
  PL_BEGIN_MESSAGEHANDLERS
  {
    PL_MESSAGE_HANDLER(plMsgDeleteGameObject, OnMsgDeleteGameObject),
    PL_MESSAGE_HANDLER(plMsgSetFloatParameter, OnMsgSetFloatParameter),
  }
  PL_END_MESSAGEHANDLERS;
  PL_BEGIN_MESSAGESENDERS
  {
    PL_MESSAGE_SENDER(m_SoundFinishedEventSender),
  }
  PL_END_MESSAGESENDERS;
  PL_BEGIN_FUNCTIONS
  {
    PL_SCRIPT_FUNCTION_PROPERTY(Play),
    PL_SCRIPT_FUNCTION_PROPERTY(Pause),
    PL_SCRIPT_FUNCTION_PROPERTY(Stop),
    PL_SCRIPT_FUNCTION_PROPERTY(FadeOut),
    PL_SCRIPT_FUNCTION_PROPERTY(StartOneShot),
    PL_SCRIPT_FUNCTION_PROPERTY(SoundCue),
    PL_SCRIPT_FUNCTION_PROPERTY(SetEventParameter, In, "ParamName", In, "Value"),
  }
  PL_END_FUNCTIONS;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

enum
{
  ShowDebugInfoFlag = 0,
  NoGlobalPitch = 1,
};

plFmodEventComponent::plFmodEventComponent()
{
  m_pEventDesc = nullptr;
  m_pEventInstance = nullptr;
  m_bPaused = false;
  m_bUseOcclusion = false;
  m_uiOcclusionThreshold = 128;
  m_uiOcclusionCollisionLayer = 0;
  m_fPitch = 1.0f;
  m_fVolume = 1.0f;
}

plFmodEventComponent::~plFmodEventComponent() = default;

void plFmodEventComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  s << m_bPaused;
  s << m_bUseOcclusion;
  s << m_uiOcclusionThreshold;
  s << m_uiOcclusionCollisionLayer;
  s << m_fPitch;
  s << m_fVolume;

  s << m_hSoundEvent;

  plOnComponentFinishedAction::StorageType type = m_OnFinishedAction;
  s << type;

  plInt32 iTimelinePosition = -1;

  if (m_pEventInstance)
  {
    FMOD_STUDIO_PLAYBACK_STATE state;
    m_pEventInstance->getPlaybackState(&state);

    if (state == FMOD_STUDIO_PLAYBACK_STARTING || state == FMOD_STUDIO_PLAYBACK_PLAYING || state == FMOD_STUDIO_PLAYBACK_SUSTAINING)
    {
      m_pEventInstance->getTimelinePosition(&iTimelinePosition);
    }
  }

  s << iTimelinePosition;
}

void plFmodEventComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  const plUInt32 uiVersion = inout_stream.GetComponentTypeVersion(GetStaticRTTI());

  auto& s = inout_stream.GetStream();

  s >> m_bPaused;

  if (uiVersion >= 3)
  {
    s >> m_bUseOcclusion;
  }

  if (uiVersion >= 4)
  {
    s >> m_uiOcclusionThreshold;
    s >> m_uiOcclusionCollisionLayer;
  }

  s >> m_fPitch;
  s >> m_fVolume;
  s >> m_hSoundEvent;

  plOnComponentFinishedAction::StorageType type;
  s >> type;
  m_OnFinishedAction = (plOnComponentFinishedAction::Enum)type;

  s >> m_iTimelinePosition;
}

void plFmodEventComponent::SetPaused(bool b)
{
  if (b == m_bPaused)
    return;

  m_bPaused = b;

  if (m_bPaused)
  {
    Pause();
  }
  else
  {
    Play();
  }
}

void plFmodEventComponent::SetUseOcclusion(bool b)
{
  if (b == GetUseOcclusion())
    return;

  m_bUseOcclusion = b;

  if (!b)
  {
    static_cast<plFmodEventComponentManager*>(GetOwningManager())->RemoveOcclusionState(m_uiOcclusionStateIndex);
    m_uiOcclusionStateIndex = plInvalidIndex;
  }
}

void plFmodEventComponent::SetOcclusionCollisionLayer(plUInt8 uiCollisionLayer)
{
  m_uiOcclusionCollisionLayer = uiCollisionLayer;
}

void plFmodEventComponent::SetOcclusionThreshold(float fThreshold)
{
  m_uiOcclusionThreshold = plMath::ColorFloatToByte(fThreshold);
}

float plFmodEventComponent::GetOcclusionThreshold() const
{
  return plMath::ColorByteToFloat(m_uiOcclusionThreshold);
}

void plFmodEventComponent::SetPitch(float f)
{
  if (f == m_fPitch)
    return;

  m_fPitch = f;

  if (m_pEventInstance != nullptr)
  {
    if (GetNoGlobalPitch())
    {
      PL_FMOD_ASSERT(m_pEventInstance->setPitch(m_fPitch));
    }
    else
    {
      PL_FMOD_ASSERT(m_pEventInstance->setPitch(m_fPitch * (float)GetWorld()->GetClock().GetSpeed()));
    }
  }
}

void plFmodEventComponent::SetVolume(float f)
{
  if (f == m_fVolume)
    return;

  m_fVolume = f;

  if (m_pEventInstance != nullptr)
  {
    PL_FMOD_ASSERT(m_pEventInstance->setVolume(m_fVolume));
  }
}

void plFmodEventComponent::SetSoundEvent(const plFmodSoundEventResourceHandle& hSoundEvent)
{
  if (m_pEventInstance)
  {
    Stop();

    PL_FMOD_ASSERT(m_pEventInstance->release());
    m_pEventInstance = nullptr;
    m_iTimelinePosition = -1;
  }

  m_hSoundEvent = hSoundEvent;
}

void plFmodEventComponent::SetShowDebugInfo(bool bShow)
{
  SetUserFlag(ShowDebugInfoFlag, bShow);
}

bool plFmodEventComponent::GetShowDebugInfo() const
{
  return GetUserFlag(ShowDebugInfoFlag);
}

void plFmodEventComponent::SetNoGlobalPitch(bool bEnable)
{
  SetUserFlag(NoGlobalPitch, bEnable);
}

bool plFmodEventComponent::GetNoGlobalPitch() const
{
  return GetUserFlag(NoGlobalPitch);
}

void plFmodEventComponent::OnSimulationStarted()
{
  if (!m_bPaused)
  {
    Play();
  }
}

void plFmodEventComponent::OnDeactivated()
{
  if (GetUseOcclusion())
  {
    static_cast<plFmodEventComponentManager*>(GetOwningManager())->RemoveOcclusionState(m_uiOcclusionStateIndex);
    m_uiOcclusionStateIndex = plInvalidIndex;
  }

  FadeOut();
}

void plFmodEventComponent::Play()
{
  if (!m_hSoundEvent.IsValid() || !IsActiveAndSimulating())
    return;

  if (m_pEventInstance == nullptr)
  {
    plResourceLock<plFmodSoundEventResource> pEvent(m_hSoundEvent, plResourceAcquireMode::BlockTillLoaded);

    if (pEvent.GetAcquireResult() == plResourceAcquireResult::MissingFallback)
      return;

    m_pEventInstance = pEvent->CreateInstance();
    if (m_pEventInstance == nullptr)
    {
      plLog::Error("Failed to start sound event '{}'.", m_hSoundEvent.GetResourceIdOrDescription());
      return;
    }
  }

  if (m_bPaused)
  {
    m_bPaused = false;
    PL_FMOD_ASSERT(m_pEventInstance->setPaused(false));
  }

  UpdateParameters(m_pEventInstance);

  FMOD_STUDIO_PLAYBACK_STATE state;
  PL_FMOD_ASSERT(m_pEventInstance->getPlaybackState(&state));

  if (state != FMOD_STUDIO_PLAYBACK_PLAYING && state != FMOD_STUDIO_PLAYBACK_SUSTAINING)
  {
    // reset this, using the value is handled outside this function
    m_iTimelinePosition = -1;

    PL_FMOD_ASSERT(m_pEventInstance->start());
  }
}

void plFmodEventComponent::Pause()
{
  if (m_bPaused)
    return;

  m_bPaused = true;

  if (m_pEventInstance == nullptr)
    return;

  PL_FMOD_ASSERT(m_pEventInstance->setPaused(m_bPaused));
}

void plFmodEventComponent::FadeOut()
{
  if (m_pEventInstance == nullptr)
    return;

  PL_FMOD_ASSERT(m_pEventInstance->stop(FMOD_STUDIO_STOP_ALLOWFADEOUT));

  PL_FMOD_ASSERT(m_pEventInstance->release());
  m_pEventInstance = nullptr;
  m_iTimelinePosition = -1;
}

void plFmodEventComponent::StartOneShot()
{
  if (!m_hSoundEvent.IsValid())
    return;

  plResourceLock<plFmodSoundEventResource> pEvent(m_hSoundEvent, plResourceAcquireMode::BlockTillLoaded);

  if (pEvent.GetAcquireResult() == plResourceAcquireResult::MissingFallback)
  {
    plLog::Debug("Cannot start one-shot sound event, resource is missing.");
    return;
  }

  if (pEvent->GetDescriptor() == nullptr)
  {
    plLog::Debug("Cannot start one-shot sound event, descriptor is null.");
    return;
  }

  bool bIsOneShot = false;
  pEvent->GetDescriptor()->isOneshot(&bIsOneShot);

  // do not start sounds that will not terminate
  if (!bIsOneShot)
  {
    plLog::Warning("plFmodEventComponent::StartOneShot: Request ignored, because sound event '{}' is not a one-shot event.", pEvent->GetResourceIdOrDescription());
    return;
  }

  FMOD::Studio::EventInstance* pEventInstance = pEvent->CreateInstance();

  if (pEventInstance == nullptr)
  {
    plLog::Debug("Cannot start one-shot sound event, instance could not be created.");
    return;
  }

  UpdateParameters(pEventInstance);

  PL_FMOD_ASSERT(pEventInstance->start());
  PL_FMOD_ASSERT(pEventInstance->release());
}

void plFmodEventComponent::Stop()
{
  if (m_pEventInstance == nullptr)
    return;

  m_iTimelinePosition = -1;
  PL_FMOD_ASSERT(m_pEventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE));
}

void plFmodEventComponent::SoundCue()
{
  if (m_pEventInstance != nullptr && m_pEventInstance->isValid())
  {
#if ((FMOD_VERSION & 0x0000FF00) >> 8 >= 2)
    PL_FMOD_ASSERT(m_pEventInstance->keyOff());
#else
    PL_FMOD_ASSERT(m_pEventInstance->triggerCue());
#endif
  }
}

void plFmodEventComponent::OnMsgDeleteGameObject(plMsgDeleteGameObject& msg)
{
  plOnComponentFinishedAction::HandleDeleteObjectMsg(msg, m_OnFinishedAction);
}

plFmodParameterId plFmodEventComponent::FindParameter(const char* szName) const
{
  if (!m_hSoundEvent.IsValid())
    return plFmodParameterId();

  plResourceLock<plFmodSoundEventResource> pEvent(m_hSoundEvent, plResourceAcquireMode::BlockTillLoaded);

  FMOD::Studio::EventDescription* pEventDesc = pEvent->GetDescriptor();
  if (pEventDesc == nullptr || !pEventDesc->isValid())
    return plFmodParameterId();

  FMOD_STUDIO_PARAMETER_DESCRIPTION paramDesc;
  if (pEventDesc->getParameterDescriptionByName(szName, &paramDesc) != FMOD_OK)
    return plFmodParameterId();

  return ConvertFmodToPlId(paramDesc.id);
}

void plFmodEventComponent::SetParameter(plFmodParameterId paramId, float fValue)
{
  if (m_pEventInstance == nullptr || !m_pEventInstance->isValid() || paramId.IsInvalidated())
    return;

  m_pEventInstance->setParameterByID(ConvertPlToFmodId(paramId), fValue);
}

float plFmodEventComponent::GetParameter(plFmodParameterId paramId) const
{
  if (m_pEventInstance == nullptr || !m_pEventInstance->isValid() || paramId.IsInvalidated())
    return 0.0f;

  float value = 0;
  float fFinalValue = 0.0f;
  m_pEventInstance->getParameterByID(ConvertPlToFmodId(paramId), &value, &fFinalValue);
  return fFinalValue;
}

void plFmodEventComponent::SetEventParameter(const char* szParamName, float fValue)
{
  plFmodParameterId paramId = FindParameter(szParamName);
  if (paramId.IsInvalidated())
    return;

  SetParameter(paramId, fValue);
}

void plFmodEventComponent::OnMsgSetFloatParameter(plMsgSetFloatParameter& ref_msg)
{
  SetEventParameter(ref_msg.m_sParameterName, ref_msg.m_fValue);
}

void plFmodEventComponent::Update()
{
  FMOD_STUDIO_PLAYBACK_STATE state = FMOD_STUDIO_PLAYBACK_FORCEINT;

  if (m_pEventInstance)
  {
    if (!m_pEventInstance->isValid())
    {
      InvalidateResource(false);
      return;
    }

    UpdateParameters(m_pEventInstance);
    if (GetUseOcclusion())
    {
      UpdateOcclusion();
    }

    PL_FMOD_ASSERT(m_pEventInstance->getPlaybackState(&state));

    if (state == FMOD_STUDIO_PLAYBACK_STOPPED)
    {
      m_iTimelinePosition = -1;

      plMsgFmodSoundFinished msg;
      m_SoundFinishedEventSender.SendEventMessage(msg, this, GetOwner());

      plOnComponentFinishedAction::HandleFinishedAction(this, m_OnFinishedAction);
    }
  }
  else if (m_iTimelinePosition >= 0 && !m_bPaused)
  {
    // Restore the event to the last playback position
    const plInt32 iTimelinePos = m_iTimelinePosition;

    Play(); // will reset m_iTimelinePosition, that's why it is copied first

    if (m_pEventInstance)
    {
      // we need to store this value again, because it is very likely during a resource reload that we
      // run into the InvalidateResource(false) case at the top of this function, right after this
      // so we can restore the timeline position again
      m_iTimelinePosition = iTimelinePos;
      m_pEventInstance->setTimelinePosition(iTimelinePos);
    }
  }

#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
  if (GetShowDebugInfo())
  {
    if (m_pEventInstance)
    {
      FMOD::Studio::EventDescription* pDesc = nullptr;
      m_pEventInstance->getDescription(&pDesc);

      bool is3D = false;
      pDesc->is3D(&is3D);
      if (is3D)
      {
        float minDistance = 0.0f;
        float maxDistance = 0.0f;
#  if ((FMOD_VERSION & 0x0000FF00) >> 8 >= 2)
        pDesc->getMinMaxDistance(&minDistance, &maxDistance);
#  else
        pDesc->getMinimumDistance(&minDistance);
        pDesc->getMaximumDistance(&maxDistance);
#  endif

        plDebugRenderer::DrawLineSphere(GetWorld(), plBoundingSphere::MakeFromCenterAndRadius(GetOwner()->GetGlobalPosition(), minDistance), plColor::Blue);
        plDebugRenderer::DrawLineSphere(GetWorld(), plBoundingSphere::MakeFromCenterAndRadius(GetOwner()->GetGlobalPosition(), maxDistance), plColor::Cyan);
      }

      char path[128];
      pDesc->getPath(path, PL_ARRAY_SIZE(path), nullptr);

      const char* szStates[] = {"PLAYING", "SUSTAINING", "STOPPED", "STARTING", "STOPPING"};

      const char* szCurrentState = "Invalid";
      if (state != FMOD_STUDIO_PLAYBACK_FORCEINT)
      {
        szCurrentState = szStates[state];
      }

      plStringBuilder sb;
      sb.SetFormat("{}\n{}", path, szCurrentState);

      if (GetUseOcclusion())
      {
        auto& occlusionState = static_cast<plFmodEventComponentManager*>(GetOwningManager())->GetOcclusionState(m_uiOcclusionStateIndex);
        sb.AppendFormat("\nOcclusion: {}", occlusionState.GetOcclusionValue(GetOcclusionThreshold()));

        plVec3 centerPos = GetOwner()->GetGlobalPosition();
        for (plUInt32 uiRayIndex = 0; uiRayIndex < PL_ARRAY_SIZE(s_InSpherePositions); ++uiRayIndex)
        {
          plVec3 targetPos = centerPos + s_InSpherePositions[uiRayIndex] * occlusionState.m_fRadius;
          plColor color = (occlusionState.m_uiRaycastHits & (1 << uiRayIndex)) ? plColor::Red : plColor::Green;
          plDebugRenderer::DrawCross(GetWorld(), targetPos, 0.1f, color);
        }
      }

      plDebugRenderer::Draw3DText(GetWorld(), sb, GetOwner()->GetGlobalPosition(), plColor::Cyan);
    }
  }
#endif
}

void plFmodEventComponent::UpdateParameters(FMOD::Studio::EventInstance* pInstance)
{
  const auto pos = GetOwner()->GetGlobalPosition();
  const auto vel = GetOwner()->GetLinearVelocity();
  const auto fwd = GetOwner()->GetGlobalRotation() * plVec3::MakeAxisX();
  const auto up = GetOwner()->GetGlobalRotation() * plVec3::MakeAxisZ();

  FMOD_3D_ATTRIBUTES attr;
  attr.position.x = pos.x;
  attr.position.y = pos.y;
  attr.position.z = pos.z;
  attr.forward.x = fwd.x;
  attr.forward.y = fwd.y;
  attr.forward.z = fwd.z;
  attr.up.x = up.x;
  attr.up.y = up.y;
  attr.up.z = up.z;
  attr.velocity.x = vel.x;
  attr.velocity.y = vel.y;
  attr.velocity.z = vel.z;

  if (GetNoGlobalPitch())
  {
    PL_FMOD_ASSERT(pInstance->setPitch(m_fPitch));
  }
  else
  {
    // have to update pitch every time, in case the clock speed changes
    PL_FMOD_ASSERT(pInstance->setPitch(m_fPitch * (float)GetWorld()->GetClock().GetSpeed()));
  }

  PL_FMOD_ASSERT(pInstance->setVolume(m_fVolume));
  PL_FMOD_ASSERT(pInstance->set3DAttributes(&attr));
}

void plFmodEventComponent::UpdateOcclusion()
{
  if (m_uiOcclusionStateIndex == plInvalidIndex)
  {
    plFmodParameterId occlusionParamId = FindParameter("Occlusion");
    if (occlusionParamId.IsInvalidated())
    {
      plLog::Warning("'Occlusion' FMOD Event Parameter could not be found.");
      m_bUseOcclusion = false;
      return;
    }

    float fRadius = 1.0f;
    float fMaxDist = 0;
    {
      FMOD::Studio::EventDescription* pDesc = nullptr;
      m_pEventInstance->getDescription(&pDesc);

      bool is3D = false;
      pDesc->is3D(&is3D);
      if (is3D)
      {
#if ((FMOD_VERSION & 0x0000FF00) >> 8 >= 2)
        pDesc->getMinMaxDistance(&fRadius, &fMaxDist);
#else
        pDesc->getMinimumDistance(&fRadius);
        pDesc->getMaximumDistance(&fMaxDist);
#endif
      }
    }

    m_uiOcclusionStateIndex = static_cast<plFmodEventComponentManager*>(GetOwningManager())->AddOcclusionState(this, occlusionParamId, fRadius);
  }

  auto& occlusionState = static_cast<plFmodEventComponentManager*>(GetOwningManager())->GetOcclusionState(m_uiOcclusionStateIndex);
  SetParameter(occlusionState.m_OcclusionParamId, occlusionState.GetOcclusionValue(GetOcclusionThreshold()));
}

void plFmodEventComponent::InvalidateResource(bool bTryToRestore)
{
  if (m_pEventInstance)
  {
    if (bTryToRestore)
    {
      FMOD_STUDIO_PLAYBACK_STATE state;
      m_pEventInstance->getPlaybackState(&state);

      if (state == FMOD_STUDIO_PLAYBACK_STARTING || state == FMOD_STUDIO_PLAYBACK_PLAYING || state == FMOD_STUDIO_PLAYBACK_SUSTAINING)
      {
        m_pEventInstance->getTimelinePosition(&m_iTimelinePosition);
      }
      else
      {
        // only reset when bTryToRestore is true, otherwise keep the old value
        m_iTimelinePosition = -1;
      }
    }

    // pointer is no longer valid!
    m_pEventInstance = nullptr;
  }
}

PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_Components_FmodEventComponent);
