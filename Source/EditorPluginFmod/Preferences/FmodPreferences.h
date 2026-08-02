#pragma once

#include <EditorFramework/Preferences/Preferences.h>
#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <Foundation/Strings/String.h>

class PL_EDITORPLUGINFMOD_DLL plFmodProjectPreferences : public plPreferences
{
  PL_ADD_DYNAMIC_REFLECTION(plFmodProjectPreferences, plPreferences);

public:
  plFmodProjectPreferences();
  ~plFmodProjectPreferences();

  void SetMute(bool bMute);
  bool GetMute() const { return m_bMute; }

  void SetVolume(float fVolume);
  float GetVolume() const { return m_fMasterVolume; }

  void SyncCVars();

private:
  void ProcessEventHandler(const plEditorEngineProcessConnection::Event& e);

  bool m_bMute = false;
  float m_fMasterVolume = 1.0f;
};
