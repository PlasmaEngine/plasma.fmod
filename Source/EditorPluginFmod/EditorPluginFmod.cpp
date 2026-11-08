#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/Actions/AssetActions.h>
#include <EditorFramework/Actions/ProjectActions.h>
#include <GuiFoundation/UIServices/DynamicStringEnum.h>
#include <GuiFoundation/Action/CommandHistoryActions.h>
#include <GuiFoundation/Action/DocumentActions.h>
#include <GuiFoundation/Action/StandardMenus.h>
#include <Foundation/Algorithm/HashingUtils.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/OSFile.h>
#include <Foundation/IO/OpenDdlUtils.h>
#include <Foundation/IO/OpenDdlWriter.h>
#include <Foundation/Utilities/ConversionUtils.h>

#include <EditorPluginFmod/Actions/FmodActions.h>
#include <EditorPluginFmod/EngineUi/FmodShellPanels.h>
#include <EditorPluginFmod/Preferences/FmodPreferences.h>
#include <EditorUi/Widgets.h>
#include <Foundation/Configuration/Plugin.h>

static void ToolsProjectEventHandler(const plToolsProjectEvent& e);
static void AssetCuratorEventHandler(const plAssetCuratorEvent& e);

static constexpr plStringView s_sAudioTriggerMapFile = ":project/RuntimeConfigs/AudioTriggerMap.ddl"_plsv;

/// The package's icon folder: the icons under their ids (":/AssetIcons/Sound_Bank.svg" is
/// "EditorIcons/AssetIcons/Sound_Bank.svg"). The plugin binary lives in <package>/Bin/<configuration>.
static plString s_sIconFolder;

static void RegisterIconFolder()
{
  plStringBuilder sPlugin, sCopy;
  plPlugin::GetPluginPaths("plEditorPluginFmod", sPlugin, sCopy, 0);

  plStringBuilder sFolder = sPlugin.GetFileDirectory();
  sFolder.AppendPath("..", "..", "EditorIcons");
  sFolder.MakeCleanPath();

  s_sIconFolder = sFolder;
  plEditorUi::AddIconFolder(s_sIconFolder);
}

static void UnregisterIconFolder()
{
  plEditorUi::RemoveIconFolder(s_sIconFolder);
  s_sIconFolder.Clear();
}

static bool IsSoundEventSubAsset(const plSubAsset& subAsset)
{
  return subAsset.m_Data.m_sSubAssetsDocumentTypeName.GetView().IsEqual_NoCase("Sound Event");
}

static bool TryBuildSoundEventResourceId(const plSubAsset& subAsset, plStringBuilder& out_sResourceId, plStringBuilder& out_sBankPath)
{
  out_sResourceId.Clear();
  out_sBankPath.Clear();

  if (subAsset.m_pAssetInfo == nullptr || subAsset.m_pAssetInfo->m_Info == nullptr)
    return false;

  for (const plString& sDependency : subAsset.m_pAssetInfo->m_Info->m_TransformDependencies)
  {
    if (!sDependency.EndsWith_NoCase(".bank") || sDependency.EndsWith_NoCase(".strings.bank"))
      continue;

    out_sBankPath = sDependency;
    break;
  }

  if (out_sBankPath.IsEmpty())
    return false;

  plStringBuilder sGuid;
  plConversionUtils::ToString(subAsset.m_Data.m_Guid, sGuid);
  out_sResourceId.SetFormat("{0}|{1}", out_sBankPath, sGuid);
  return true;
}

static void AddTriggerMapEntry(plMap<plString, plString>& inout_triggerLookup, plStringView sLookupName, plStringView sResourceId)
{
  if (sLookupName.IsEmpty() || sResourceId.IsEmpty())
    return;

  const plString sLookupKey(sLookupName);
  const plString sResourceValue(sResourceId);

  if (const auto it = inout_triggerLookup.Find(sLookupKey); it.IsValid())
  {
    if (!it.Value().IsEqual(sResourceValue))
    {
      plLog::Warning("Audio trigger map key '{0}' already mapped to '{1}', keeping existing value and skipping '{2}'.",
        sLookupName, it.Value(), sResourceId);
    }
    return;
  }

  inout_triggerLookup.Insert(sLookupKey, sResourceValue);
}

static void GenerateAudioTriggerMap()
{
  static plUInt64 s_uiLastTriggerMapFingerprint = 0;
  static bool s_bHasLastTriggerMapFingerprint = false;

  plMap<plString, plString> triggerLookup;
  plSet<plString> bankPaths;

  const auto knownSubAssets = plAssetCurator::GetSingleton()->GetKnownSubAssets();
  for (auto it = knownSubAssets->GetIterator(); it.IsValid(); ++it)
  {
    const plSubAsset& subAsset = it.Value();

    if (!IsSoundEventSubAsset(subAsset))
      continue;

    plStringBuilder sResourceId;
    plStringBuilder sBankPath;
    if (!TryBuildSoundEventResourceId(subAsset, sResourceId, sBankPath))
      continue;

    bankPaths.Insert(sBankPath);

    AddTriggerMapEntry(triggerLookup, subAsset.m_Data.m_sName, sResourceId);

    plStringBuilder sIdentifier;
    subAsset.GetSubAssetIdentifier(sIdentifier);
    AddTriggerMapEntry(triggerLookup, sIdentifier, sResourceId);
  }

  plUInt64 uiFingerprint = 0;
  for (auto it = triggerLookup.GetIterator(); it.IsValid(); ++it)
  {
    uiFingerprint = plHashingUtils::xxHash64String(it.Key(), uiFingerprint);
    uiFingerprint = plHashingUtils::xxHash64String(it.Value(), uiFingerprint);
  }
  for (const plString& sBankPath : bankPaths)
  {
    uiFingerprint = plHashingUtils::xxHash64String(sBankPath, uiFingerprint);
  }

  if (s_bHasLastTriggerMapFingerprint && uiFingerprint == s_uiLastTriggerMapFingerprint)
  {
    return;
  }

  if (!plToolsProject::GetSingleton())
    return;

  plStringBuilder sRuntimeConfigDir = plToolsProject::GetSingleton()->GetProjectDirectory();
  sRuntimeConfigDir.AppendPath("RuntimeConfigs");
  if (plOSFile::CreateDirectoryStructure(sRuntimeConfigDir).Failed())
  {
    plLog::Warning("Failed to create FMOD trigger map directory '{0}'.", sRuntimeConfigDir);
    return;
  }

  plStringBuilder sAbsoluteMapPath = sRuntimeConfigDir;
  sAbsoluteMapPath.AppendPath("AudioTriggerMap.ddl");

  plFileWriter file;
  plStringBuilder sWrittenPath = s_sAudioTriggerMapFile;
  if (file.Open(s_sAudioTriggerMapFile).Failed())
  {
    if (file.Open(sAbsoluteMapPath).Failed())
    {
      plLog::Warning("Failed to write FMOD trigger map file to '{0}' and fallback '{1}'.", s_sAudioTriggerMapFile, sAbsoluteMapPath);
      return;
    }

    sWrittenPath = sAbsoluteMapPath;
    plLog::Warning("Writing FMOD trigger map through absolute fallback path '{0}'.", sAbsoluteMapPath);
  }

  plOpenDdlWriter ddl;
  ddl.SetOutputStream(&file);
  ddl.BeginObject("AudioTriggerMap");
  {
    for (auto it = triggerLookup.GetIterator(); it.IsValid(); ++it)
    {
      ddl.BeginObject("Trigger");
      {
        plOpenDdlUtils::StoreString(ddl, it.Key(), "Name");
        plOpenDdlUtils::StoreString(ddl, it.Value(), "Resource");
      }
      ddl.EndObject();
    }

    for (const plString& sBankPath : bankPaths)
    {
      ddl.BeginObject("Bank");
      {
        plOpenDdlUtils::StoreString(ddl, sBankPath, "Path");
      }
      ddl.EndObject();
    }
  }
  ddl.EndObject();

  plLog::Info("Generated FMOD trigger map '{0}' with {1} trigger entries and {2} bank entries.",
    sWrittenPath, triggerLookup.GetCount(), bankPaths.GetCount());

  s_uiLastTriggerMapFingerprint = uiFingerprint;
  s_bHasLastTriggerMapFingerprint = true;
}

static void UpdateAudioSystemTriggerEnum()
{
  auto& triggerEnum = plDynamicStringEnum::CreateDynamicEnum("AudioSystem.Trigger");
  triggerEnum.Clear();

  const auto knownSubAssets = plAssetCurator::GetSingleton()->GetKnownSubAssets();
  for (auto it = knownSubAssets->GetIterator(); it.IsValid(); ++it)
  {
    const plSubAsset& subAsset = it.Value();

    if (!IsSoundEventSubAsset(subAsset))
      continue;

    plStringBuilder sIdentifier;
    subAsset.GetSubAssetIdentifier(sIdentifier);
    if (sIdentifier.IsEmpty())
      continue;

    // Store the full sub-asset identifier in the enum value so runtime playback
    // can resolve directly through FMOD resources.
    triggerEnum.AddValidValue(sIdentifier);
  }

  triggerEnum.SortValues();
}

static void UpdateAudioSystemBankEnum()
{
  auto& bankEnum = plDynamicStringEnum::CreateDynamicEnum("AudioSystem.Bank");
  bankEnum.Clear();

  const auto knownSubAssets = plAssetCurator::GetSingleton()->GetKnownSubAssets();
  for (auto it = knownSubAssets->GetIterator(); it.IsValid(); ++it)
  {
    const plSubAsset& subAsset = it.Value();

    if (!subAsset.m_bMainAsset)
      continue;

    if (subAsset.m_pAssetInfo == nullptr || subAsset.m_pAssetInfo->m_pDocumentTypeDescriptor == nullptr)
      continue;

    if (subAsset.m_pAssetInfo->m_pDocumentTypeDescriptor->m_sDocumentTypeName != "Sound Bank")
      continue;

    plStringBuilder sIdentifier;
    subAsset.GetSubAssetIdentifier(sIdentifier);
    if (sIdentifier.IsEmpty())
      continue;

    bankEnum.AddValidValue(sIdentifier);
  }

  bankEnum.SortValues();
}

void OnLoadPlugin()
{
  RegisterIconFolder();

  plToolsProject::GetSingleton()->s_Events.AddEventHandler(ToolsProjectEventHandler);
  plAssetCurator::GetSingleton()->m_Events.AddEventHandler(AssetCuratorEventHandler);

  // Mesh
  {
    const char* szMenuBar = "SoundBankAssetMenuBar";
    plActionMapManager::RegisterActionMap(szMenuBar).IgnoreResult();
    plStandardMenus::MapActions(szMenuBar, plStandardMenuTypes::Default | plStandardMenuTypes::Edit);
    plProjectActions::MapActions(szMenuBar);
    plDocumentActions::MapMenuActions(szMenuBar);
    plAssetActions::MapMenuActions(szMenuBar);
    plCommandHistoryActions::MapActions(szMenuBar);

    // Tool Bar
    {
      const char* szToolBar = "SoundBankAssetToolBar";
      plActionMapManager::RegisterActionMap(szToolBar).IgnoreResult();
      plDocumentActions::MapToolbarActions(szToolBar);
      plCommandHistoryActions::MapActions(szToolBar, "");
    }
  }

  // Sound Event: the window sets these maps, so they must exist before one opens
  {
    const char* szMenuBar = "SoundEventAssetMenuBar";
    plActionMapManager::RegisterActionMap(szMenuBar).IgnoreResult();
    plStandardMenus::MapActions(szMenuBar, plStandardMenuTypes::Default | plStandardMenuTypes::Edit);
    plProjectActions::MapActions(szMenuBar);
    plDocumentActions::MapMenuActions(szMenuBar);
    plAssetActions::MapMenuActions(szMenuBar);
    plCommandHistoryActions::MapActions(szMenuBar);

    const char* szToolBar = "SoundEventAssetToolBar";
    plActionMapManager::RegisterActionMap(szToolBar).IgnoreResult();
    plDocumentActions::MapToolbarActions(szToolBar);
    plCommandHistoryActions::MapActions(szToolBar, "");
  }

  // Scene
  {
    // Menu Bar
    {
      plFmodActions::RegisterActions();
      plFmodActions::MapMenuActions("EditorPluginScene_DocumentMenuBar");
      plFmodActions::MapMenuActions("EditorPluginScene_Scene2MenuBar");

      // Project > Packages > FMOD > FMOD Project Settings
      plFmodActions::MapPluginMenuActions("EditorPluginScene_DocumentMenuBar");
      plFmodActions::MapPluginMenuActions("EditorPluginScene_Scene2MenuBar");

      plFmodActions::MapToolbarActions("EditorPluginScene_DocumentToolBar");
      plFmodActions::MapToolbarActions("EditorPluginScene_Scene2ToolBar");
    }
  }

  // the sound windows' panels in the engine shell
  plFmodShellPanels::Register();
}

void OnUnloadPlugin()
{
  UnregisterIconFolder();
  plFmodShellPanels::Unregister();
  plAssetCurator::GetSingleton()->m_Events.RemoveEventHandler(AssetCuratorEventHandler);
  plFmodActions::UnregisterActions();
  plToolsProject::GetSingleton()->s_Events.RemoveEventHandler(ToolsProjectEventHandler);
}

static void ToolsProjectEventHandler(const plToolsProjectEvent& e)
{
  if (e.m_Type == plToolsProjectEvent::Type::ProjectOpened)
  {
    UpdateAudioSystemTriggerEnum();
    UpdateAudioSystemBankEnum();
    GenerateAudioTriggerMap();

    plFmodProjectPreferences* pPreferences = plPreferences::QueryPreferences<plFmodProjectPreferences>();
    pPreferences->SyncCVars();
  }
}

static void AssetCuratorEventHandler(const plAssetCuratorEvent& e)
{
  switch (e.m_Type)
  {
    case plAssetCuratorEvent::Type::AssetAdded:
    case plAssetCuratorEvent::Type::AssetRemoved:
    case plAssetCuratorEvent::Type::AssetUpdated:
    case plAssetCuratorEvent::Type::AssetListReset:
      UpdateAudioSystemTriggerEnum();
      UpdateAudioSystemBankEnum();
      GenerateAudioTriggerMap();
      break;
    default:
      break;
  }
}

PL_PLUGIN_ON_LOADED()
{
  OnLoadPlugin();
}

PL_PLUGIN_ON_UNLOADED()
{
  OnUnloadPlugin();
}
