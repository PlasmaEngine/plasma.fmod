#include <FmodPlugin/FmodPluginPCH.h>

#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <FmodPlugin/Components/FmodListenerComponent.h>
#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodSingleton.h>

plFmodListenerComponentManager::plFmodListenerComponentManager(plWorld* pWorld)
  : plComponentManager(pWorld)
{
}

void plFmodListenerComponentManager::Initialize()
{
  SUPER::Initialize();

  {
    auto desc = PL_CREATE_MODULE_UPDATE_FUNCTION_DESC(plFmodListenerComponentManager::UpdateListeners, this);
    desc.m_Phase = plWorldModule::UpdateFunctionDesc::Phase::PostTransform;
    desc.m_bOnlyUpdateWhenSimulating = true;

    this->RegisterUpdateFunction(desc);
  }
}

void plFmodListenerComponentManager::UpdateListeners(const plWorldModule::UpdateContext& context)
{
  for (auto it = this->m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
  {
    ComponentType* pComponent = it;
    if (pComponent->IsActiveAndInitialized())
    {
      pComponent->Update();
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
PL_BEGIN_COMPONENT_TYPE(plFmodListenerComponent, 1, plComponentMode::Static)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("ListenerIndex", m_uiListenerIndex),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plFmodListenerComponent::plFmodListenerComponent() = default;
plFmodListenerComponent::~plFmodListenerComponent() = default;

void plFmodListenerComponent::SerializeComponent(plWorldWriter& inout_stream) const
{
  SUPER::SerializeComponent(inout_stream);

  auto& s = inout_stream.GetStream();

  s << m_uiListenerIndex;
}

void plFmodListenerComponent::DeserializeComponent(plWorldReader& inout_stream)
{
  SUPER::DeserializeComponent(inout_stream);
  // const plUInt32 uiVersion = stream.GetComponentTypeVersion(GetStaticRTTI());

  auto& s = inout_stream.GetStream();

  s >> m_uiListenerIndex;
}

void plFmodListenerComponent::Update()
{
  const auto pos = GetOwner()->GetGlobalPosition();
  const auto vel = GetOwner()->GetLinearVelocity();
  const auto fwd = (GetOwner()->GetGlobalRotation() * plVec3::MakeAxisX()).GetNormalized();
  const auto up = (GetOwner()->GetGlobalRotation() * plVec3::MakeAxisZ()).GetNormalized();

  plFmod::GetSingleton()->SetListener(m_uiListenerIndex, pos, fwd, up, vel);
}

PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_Components_FmodListenerComponent);
