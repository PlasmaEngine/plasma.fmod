#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/Actions/FmodActions.h>
#include <EditorPluginFmod/Dialogs/FmodProjectSettingsEngineDlg.h>
#include <EditorPluginFmod/Preferences/FmodPreferences.h>
#include <GuiFoundation/Action/ActionManager.h>

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodAction, 0, plRTTINoAllocator)
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodSliderAction, 1, plRTTINoAllocator)
PL_END_DYNAMIC_REFLECTED_TYPE;

plActionDescriptorHandle plFmodActions::s_hCategoryFmod;
plActionDescriptorHandle plFmodActions::s_hProjectSettings;
plActionDescriptorHandle plFmodActions::s_hMuteSound;
plActionDescriptorHandle plFmodActions::s_hMasterVolume;

void plFmodActions::RegisterActions()
{
  s_hCategoryFmod = PL_REGISTER_CATEGORY("FMOD");
  s_hProjectSettings = PL_REGISTER_ACTION_1("FMOD.Settings.Project", plActionScope::Document, "FMOD", "", plFmodAction, plFmodAction::ActionType::ProjectSettings);
  s_hMuteSound = PL_REGISTER_ACTION_1("FMOD.Mute", plActionScope::Document, "FMOD", "", plFmodAction, plFmodAction::ActionType::MuteSound);
  s_hMasterVolume = PL_REGISTER_ACTION_1("FMOD.MasterVolume", plActionScope::Document, "FMOD", "", plFmodSliderAction, plFmodSliderAction::ActionType::MasterVolume);
}

void plFmodActions::UnregisterActions()
{
  plActionManager::UnregisterAction(s_hCategoryFmod);
  plActionManager::UnregisterAction(s_hProjectSettings);
  plActionManager::UnregisterAction(s_hMuteSound);
  plActionManager::UnregisterAction(s_hMasterVolume);
}

void plFmodActions::MapPluginMenuActions(plStringView sMapping)
{
  plActionMap* pMap = plActionMapManager::GetActionMap(sMapping);
  PL_ASSERT_DEV(pMap != nullptr, "Mapping the actions failed!");

  pMap->MapAction(s_hCategoryFmod, "G.Plugins.Settings", 9.0f);
  pMap->MapAction(s_hProjectSettings, "G.Plugins.Settings", "FMOD", 0.0f);
}

void plFmodActions::MapMenuActions(plStringView sMapping)
{
  plActionMap* pMap = plActionMapManager::GetActionMap(sMapping);
  PL_ASSERT_DEV(pMap != nullptr, "Mapping the actions failed!");

  pMap->MapAction(s_hCategoryFmod, "G.Scene", 5.0f);
  pMap->MapAction(s_hMuteSound, "G.Scene", "FMOD", 0.0f);
  pMap->MapAction(s_hMasterVolume, "G.Scene", "FMOD", 1.0f);
}

void plFmodActions::MapToolbarActions(plStringView sMapping)
{
  plActionMap* pSceneMap = plActionMapManager::GetActionMap(sMapping);
  PL_ASSERT_DEV(pSceneMap != nullptr, "Mapping the actions failed!");

  pSceneMap->MapAction(s_hCategoryFmod, "", 12.0f);
  pSceneMap->MapAction(s_hMuteSound, "FMOD", 0.0f);
}

plFmodAction::plFmodAction(const plActionContext& context, const char* szName, ActionType type)
  : plButtonAction(context, szName, false, "")
{
  m_Type = type;

  switch (m_Type)
  {
    case ActionType::ProjectSettings:
      SetIconPath(":/AssetIcons/Sound_Event.svg");
      break;

    case ActionType::MuteSound:
    {
      SetCheckable(true);

      plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();
      pPreferences->m_ChangedEvent.AddEventHandler(plMakeDelegate(&plFmodAction::OnPreferenceChange, this));

      if (pPreferences->GetMute())
        SetIconPath(":/Icons/SoundOff.svg");
      else
        SetIconPath(":/Icons/SoundOn.svg");

      SetChecked(pPreferences->GetMute());
    }
    break;
  }
}

plFmodAction::~plFmodAction()
{
  if (m_Type == ActionType::MuteSound)
  {
    plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();
    pPreferences->m_ChangedEvent.RemoveEventHandler(plMakeDelegate(&plFmodAction::OnPreferenceChange, this));
  }
}

void plFmodAction::Execute(const plVariant& value)
{
  if (m_Type == ActionType::ProjectSettings)
  {
    plFmodProjectSettingsEngineDlg::Open();
  }

  if (m_Type == ActionType::MuteSound)
  {
    plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();
    pPreferences->SetMute(!pPreferences->GetMute());

    if (GetContext().m_pDocument)
    {
      GetContext().m_pDocument->ShowDocumentStatus(plFmt("Sound is {}", pPreferences->GetMute() ? "muted" : "on"));
    }
  }
}

void plFmodAction::OnPreferenceChange(plPreferences* pref)
{
  plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();

  if (m_Type == ActionType::MuteSound)
  {
    if (pPreferences->GetMute())
      SetIconPath(":/Icons/SoundOff.svg");
    else
      SetIconPath(":/Icons/SoundOn.svg");

    SetChecked(pPreferences->GetMute());
  }
}

//////////////////////////////////////////////////////////////////////////

plFmodSliderAction::plFmodSliderAction(const plActionContext& context, const char* szName, ActionType type)
  : plSliderAction(context, szName)
{
  m_Type = type;

  switch (m_Type)
  {
    case ActionType::MasterVolume:
    {
      plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();

      pPreferences->m_ChangedEvent.AddEventHandler(plMakeDelegate(&plFmodSliderAction::OnPreferenceChange, this));

      SetRange(0, 20);
    }
    break;
  }

  UpdateState();
}

plFmodSliderAction::~plFmodSliderAction()
{
  switch (m_Type)
  {
    case ActionType::MasterVolume:
    {
      plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();
      pPreferences->m_ChangedEvent.RemoveEventHandler(plMakeDelegate(&plFmodSliderAction::OnPreferenceChange, this));
    }
    break;
  }
}

void plFmodSliderAction::Execute(const plVariant& value)
{
  const plInt32 iValue = value.Get<plInt32>();

  switch (m_Type)
  {
    case ActionType::MasterVolume:
    {
      plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();

      pPreferences->SetVolume(iValue / 20.0f);

      if (GetContext().m_pDocument)
      {
        GetContext().m_pDocument->ShowDocumentStatus(plFmt("Sound Volume: {}%%", (int)(pPreferences->GetVolume() * 100.0f)));
      }
    }
    break;
  }
}

void plFmodSliderAction::OnPreferenceChange(plPreferences* pref)
{
  UpdateState();
}

void plFmodSliderAction::UpdateState()
{
  switch (m_Type)
  {
    case ActionType::MasterVolume:
    {
      plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();

      SetValue(plMath::Clamp((plInt32)(pPreferences->GetVolume() * 20.0f), 0, 20));
    }
    break;
  }
}
