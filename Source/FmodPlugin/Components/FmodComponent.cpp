#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/Components/FmodComponent.h>
#include <FmodPlugin/FmodIncludes.h>

// clang-format off
PL_BEGIN_ABSTRACT_COMPONENT_TYPE(plFmodComponent, 1)
{
  PL_BEGIN_ATTRIBUTES
  {
    new plCategoryAttribute("Audio/FMOD"),
  }
  PL_END_ATTRIBUTES;
}
PL_END_ABSTRACT_COMPONENT_TYPE;
// clang-format on

plFmodComponent::plFmodComponent() = default;
plFmodComponent::~plFmodComponent() = default;


PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_Components_FmodComponent);
