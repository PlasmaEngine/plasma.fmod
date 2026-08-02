#pragma once

#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <Foundation/Configuration/CVar.h>
#include <GuiFoundation/Action/BaseActions.h>
#include <GuiFoundation/GuiFoundationDLL.h>

class plPreferences;

class PL_EDITORPLUGINFMOD_DLL plFmodActions
{
public:
  static void RegisterActions();
  static void UnregisterActions();

  static void MapPluginMenuActions(plStringView sMapping);
  static void MapMenuActions(plStringView sMapping);
  static void MapToolbarActions(plStringView sMapping);

  static plActionDescriptorHandle s_hCategoryFmod;
  static plActionDescriptorHandle s_hProjectSettings;
  static plActionDescriptorHandle s_hMuteSound;
  static plActionDescriptorHandle s_hMasterVolume;
};


class PL_EDITORPLUGINFMOD_DLL plFmodAction : public plButtonAction
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodAction, plButtonAction);

public:
  enum class ActionType
  {
    ProjectSettings,
    MuteSound,
  };

  plFmodAction(const plActionContext& context, const char* szName, ActionType type);
  ~plFmodAction();

  virtual void Execute(const plVariant& value) override;

private:
  void OnPreferenceChange(plPreferences* pref);

  ActionType m_Type;
};

class PL_EDITORPLUGINFMOD_DLL plFmodSliderAction : public plSliderAction
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodSliderAction, plSliderAction);

public:
  enum class ActionType
  {
    MasterVolume,
  };

  plFmodSliderAction(const plActionContext& context, const char* szName, ActionType type);
  ~plFmodSliderAction();

  virtual void Execute(const plVariant& value) override;

private:
  void OnPreferenceChange(plPreferences* pref);
  void UpdateState();

  ActionType m_Type;
};
