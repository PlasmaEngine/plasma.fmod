#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/Dialogs/FmodProjectSettings.h>
#include <GuiFoundation/Dialogs/FileDialog.h>
#include <GuiFoundation/EngineUi/EnginePrompt.h>

void plFmodProjectSettings::Load()
{
  m_Configs.Load().IgnoreResult();

  m_ConfigsOld = m_Configs;
}

plString plFmodProjectSettings::GetInitialPlatform() const
{
  if (m_Configs.m_AssetProfiles.Contains("Desktop"))
    return "Desktop";

  return GetFirstPlatform();
}

plString plFmodProjectSettings::GetFirstPlatform() const
{
  if (m_Configs.m_AssetProfiles.IsEmpty())
    return {};

  return m_Configs.m_AssetProfiles.GetIterator().Key();
}

bool plFmodProjectSettings::AskAndSave() const
{
  if (m_ConfigsOld.m_AssetProfiles == m_Configs.m_AssetProfiles)
    return true;

  if (plEditorUiServices::MessageBoxQuestion("Save the changes to the FMOD configuration?\nYou need to reload the project for the changes to take effect.", plEditorUiMessageButton::Yes | plEditorUiMessageButton::No, plEditorUiMessageButton::No) != plEditorUiMessageButton::Yes)
    return true;

  plEditorApp::GetSingleton()->AddReloadProjectRequiredReason("FMOD configuration was modified.");

  if (m_Configs.Save().Failed())
  {
    plStringBuilder sError;
    sError.SetFormat("Failed to save the FMOD configuration file\n'{0}'", plFmodAssetProfiles::s_sConfigFile);

    plEditorUiServices::MessageBoxWarning(sError);
    return false;
  }

  return true;
}

plString plFmodProjectSettings::AskAndAddPlatform()
{
  plStringBuilder sName = plEnginePrompt::GetText("Add Platform", "Platform Name:");
  sName.Trim(" \t\r\n");

  if (sName.IsEmpty())
    return {};

  const plString sPlatform = sName;

  // a new platform starts with default values
  if (!m_Configs.m_AssetProfiles.Contains(sPlatform))
    m_Configs.m_AssetProfiles.Insert(sPlatform, plFmodConfiguration());

  return sPlatform;
}

bool plFmodProjectSettings::PickMasterBank(plString& out_sPath)
{
  static plString sLastPath = plToolsProject::GetSingleton()->GetProjectDirectory();
  const plString sFile = plFileDialog::GetOpenFileName("Select Master Sound Bank", sLastPath, "Sound Banks (*.bank)");

  if (sFile.IsEmpty())
    return false;

  plStringBuilder sRelative = sFile;

  if (!plEditorApp::GetSingleton()->MakePathDataDirectoryRelative(sRelative))
  {
    plEditorUiServices::MessageBoxWarning("<html>The selected sound bank is not in a data directory of the project.<br><br>Either choose another sound bank or add the necessary folder as a <a href='https://plengine.net/pages/docs/projects/data-directories.html'>data directory</a> to the project.</html>");
    return false;
  }

  sLastPath = sFile;

  if (sRelative.EndsWith_NoCase(".strings.bank"))
  {
    plEditorUiServices::MessageBoxWarning("The strings sound bank cannot be selected as the master sound bank.");
    return false;
  }

  out_sPath = sRelative;
  return true;
}
