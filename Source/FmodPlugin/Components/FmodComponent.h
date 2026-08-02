#pragma once

#include <Core/World/Component.h>
#include <Core/World/World.h>
#include <FmodPlugin/FmodPluginDLL.h>

/// \brief Base class for all FMOD components, such that they all have a common ancestor
class PL_FMODPLUGIN_DLL plFmodComponent : public plComponent
{
  PL_DECLARE_ABSTRACT_COMPONENT_TYPE(plFmodComponent, plComponent);

  //////////////////////////////////////////////////////////////////////////
  // plComponent

public:
  virtual void SerializeComponent(plWorldWriter& inout_stream) const override {}
  virtual void DeserializeComponent(plWorldReader& inout_stream) override {}

  //////////////////////////////////////////////////////////////////////////
  // plFmodComponent

public:
  plFmodComponent();
  ~plFmodComponent();

private:
  virtual void plFmodComponentIsAbstract() = 0; // abstract classes are not shown in the UI, since this class has no other abstract functions so far, this is a dummy
};
