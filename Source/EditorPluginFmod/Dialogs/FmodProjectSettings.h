#pragma once

#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <FmodPlugin/FmodSingleton.h>

/// \brief What the FMOD Project Settings dialog edits: the platform configurations in RuntimeConfigs/FmodConfig.ddl,
/// with the prompt, the file picker and the save question around them.
class plFmodProjectSettings
{
public:
  /// Reads the configuration file; without one there are no platforms.
  void Load();

  /// The platform the dialog shows first: "Desktop" when it exists, otherwise the first one, otherwise none.
  plString GetInitialPlatform() const;

  /// The first platform in name order; empty without platforms.
  plString GetFirstPlatform() const;

  /// OK: when a configuration changed, asks whether to save them ("No" closes without saving) and saves on "Yes".
  /// Returns false when saving failed, so the dialog stays open.
  bool AskAndSave() const;

  /// "Add Platform": asks for a name. A new platform starts with default values; an existing one is only returned.
  /// Returns the name, or an empty string when cancelled.
  plString AskAndAddPlatform();

  /// The file picker for the master bank: a data-directory relative path, never the strings bank. Warns and returns
  /// false otherwise.
  static bool PickMasterBank(plString& out_sPath);

  plFmodAssetProfiles m_Configs;

private:
  plFmodAssetProfiles m_ConfigsOld;
};
