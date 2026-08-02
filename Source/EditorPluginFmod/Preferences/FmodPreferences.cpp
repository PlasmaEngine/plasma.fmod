#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/Preferences/FmodPreferences.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plFmodProjectPreferences, 1, plRTTIDefaultAllocator<plFmodProjectPreferences>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("Mute", m_bMute),
    PL_MEMBER_PROPERTY("Volume", m_fMasterVolume)->AddAttributes(new plDefaultValueAttribute(1.0f), new plClampValueAttribute(0.0f, 1.0f)),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plFmodProjectPreferences::plFmodProjectPreferences()
  : plPreferences(Domain::Project, "FMOD")
{
  plEditorEngineProcessConnection::s_Events.AddEventHandler(plMakeDelegate(&plFmodProjectPreferences::ProcessEventHandler, this));
}

plFmodProjectPreferences::~plFmodProjectPreferences()
{
  plEditorEngineProcessConnection::s_Events.RemoveEventHandler(plMakeDelegate(&plFmodProjectPreferences::ProcessEventHandler, this));
}

void plFmodProjectPreferences::SetMute(bool bMute)
{
  m_bMute = bMute;

  SyncCVars();
}

void plFmodProjectPreferences::SetVolume(float fVolume)
{
  m_fMasterVolume = plMath::Clamp(fVolume, 0.0f, 1.0f);

  SyncCVars();
}

void plFmodProjectPreferences::SyncCVars()
{
  TriggerPreferencesChangedEvent();

  {
    plChangeCVarMsgToEngine msg;
    msg.m_sCVarName = "FMOD.Mute";
    msg.m_NewValue = m_bMute;

    plEditorEngineProcessConnection::GetSingleton()->SendMessage(&msg);
  }

  {
    plChangeCVarMsgToEngine msg;
    msg.m_sCVarName = "FMOD.MasterVolume";
    msg.m_NewValue = m_fMasterVolume;

    plEditorEngineProcessConnection::GetSingleton()->SendMessage(&msg);
  }
}

void plFmodProjectPreferences::ProcessEventHandler(const plEditorEngineProcessConnection::Event& e)
{
  if (e.m_Type == plEditorEngineProcessConnection::Event::Type::ProcessRestarted)
  {
    SyncCVars();
  }
}
