#pragma once

#include <FmodPlugin/Components/FmodComponent.h>

class plFmodListenerComponentManager : public plComponentManager<class plFmodListenerComponent, plBlockStorageType::Compact>
{
public:
  plFmodListenerComponentManager(plWorld* pWorld);

  virtual void Initialize() override;

private:
  void UpdateListeners(const plWorldModule::UpdateContext& context);
};

//////////////////////////////////////////////////////////////////////////

/// \brief Represents the position of the sound listener
class PL_FMODPLUGIN_DLL plFmodListenerComponent : public plFmodComponent
{
  PL_DECLARE_COMPONENT_TYPE(plFmodListenerComponent, plFmodComponent, plFmodListenerComponentManager);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override;
  virtual void DeserializeComponent(plWorldReader& inout_stream) override;

  //////////////////////////////////////////////////////////////////////////
  // plFmodComponent

private:
  virtual void plFmodComponentIsAbstract() override {}

  //////////////////////////////////////////////////////////////////////////
  // plFmodListenerComponent

public:
  plFmodListenerComponent();
  ~plFmodListenerComponent();

  plUInt8 m_uiListenerIndex = 0; // [ property ]

protected:
  void Update();
};
