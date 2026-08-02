#pragma once

#include <AudioSystem/Core/AudioSystemData.h>
#include <Core/ResourceManager/ResourceHandle.h>
#include <Core/World/Declarations.h>
#include <FmodPlugin/FmodPluginDLL.h>
#include <Foundation/Configuration/Plugin.h>
#include <Foundation/Configuration/Singleton.h>
#include <Foundation/Math/Transform.h>
#include <Foundation/Types/UniquePtr.h>

struct plGameApplicationExecutionEvent;
class plOpenDdlWriter;
class plOpenDdlReaderElement;
using plDataBuffer = plDynamicArray<plUInt8>;

using plFmodSoundBankResourceHandle = plTypedResourceHandle<class plFmodSoundBankResource>;

/// \brief Abstraction of FMOD_SPEAKERMODE
enum class plFmodSpeakerMode : plUInt8
{
  ModeStereo,
  Mode5Point1,
  Mode7Point1,
};

/// \brief The FMOD configuration to be used on a specific platform
struct PL_FMODPLUGIN_DLL plFmodConfiguration
{
  plString m_sMasterSoundBank;
  plFmodSpeakerMode m_SpeakerMode = plFmodSpeakerMode::Mode5Point1; ///< This must be set to what is configured in FMOD Studio for the
                                                                    ///< target platform. Using anything else is incorrect.
  plUInt16 m_uiVirtualChannels = 32;                                ///< See FMOD::Studio::System::initialize
  plUInt32 m_uiSamplerRate = 0;                                     ///< See FMOD::System::setSoftwareFormat

  void Save(plOpenDdlWriter& ref_ddl) const;
  void Load(const plOpenDdlReaderElement& ddl);

  bool operator==(const plFmodConfiguration& rhs) const;
  bool operator!=(const plFmodConfiguration& rhs) const { return !operator==(rhs); }
};

/// \brief All available FMOD platform configurations
struct PL_FMODPLUGIN_DLL plFmodAssetProfiles
{
  static constexpr const plStringView s_sConfigFile = ":project/RuntimeConfigs/FmodConfig.ddl"_plsv;

  plResult Save(plStringView sFile = s_sConfigFile) const;
  plResult Load(plStringView sFile = s_sConfigFile);

  plMap<plString, plFmodConfiguration> m_AssetProfiles;
};

class PL_FMODPLUGIN_DLL plFmod
{
  PL_DECLARE_SINGLETON(plFmod);

public:
  plFmod();

  void Startup();
  void Shutdown();

  FMOD::Studio::System* GetStudioSystem() const { return m_pStudioSystem; }
  FMOD::System* GetLowLevelSystem() const { return m_pLowLevelSystem; }

  /// \brief Can be called before startup to load the FMOD configs from a different file.
  /// Otherwise will automatically be loaded by FMOD startup with the default path.
  void LoadConfiguration(plStringView sFile);

  /// \brief By default the FMOD integration will auto-detect the platform (and thus the config) to use.
  /// Calling this before startup allows to override which configuration is used.
  void SetOverridePlatform(plStringView sPlatform);

  /// \brief Updates FMOD state.
  void UpdateSound();

  /// \brief Adjusts the master volume. This affects all sounds, with no exception. Value must be between 0.0f and 1.0f.
  void SetMasterChannelVolume(float fVolume);
  float GetMasterChannelVolume() const;

  /// \brief Allows to mute all sounds. Useful for when the application goes to a background state.
  void SetMasterChannelMute(bool bMute);
  bool GetMasterChannelMute() const;

  /// \brief Allows to pause all sounds. Useful for when the application goes to a background state and you want to pause all sounds,
  /// instead of mute them.
  void SetMasterChannelPaused(bool bPaused);
  bool GetMasterChannelPaused() const;

  /// \brief Specifies the volume for a VCA ('Voltage Control Amplifier').
  ///
  /// This is used to control the volume of high level sound groups, such as 'Effects', 'Music', 'Ambiance or 'Speech'.
  /// Note that the FMOD strings banks are never loaded, so the given string must be a GUID (FMOD Studio -> Copy GUID).
  void SetSoundGroupVolume(plStringView sVcaGroupGuid, float fVolume);
  float GetSoundGroupVolume(plStringView sVcaGroupGuid) const;
  void UpdateSoundGroupVolumes();

  /// \brief Default is 1. Allows to set how many virtual listeners the sound is mixed for (split screen game play).
  void SetNumListeners(plUInt8 uiNumListeners);
  plUInt8 GetNumListeners() const;

  /// \brief Configures how many reverb ('EAX') volumes are being blended/mixed for a sound.
  ///
  /// The number is clamped between 0 and 4. 0 Means all environmental effects are disabled for all sound sources.
  /// 1 means only the most important reverb is applied. 2, 3 and 4 allow to add more fidelity, but will cost more CPU resources.
  ///
  /// The default is currently 4.
  void SetNumBlendedReverbVolumes(plUInt8 uiNumBlendedVolumes);

  /// \brief See SetNumBlendedReverbVolumes()
  plUInt8 GetNumBlendedReverbVolumes() const { return m_uiNumBlendedVolumes; }

  /// \brief Sets the global parameter with the given name to a the desired value.
  ///
  /// Global parameters affect any sound event that uses them, no matter on which instance they play.
  /// They are convenient to use for general things like passing the game state to the sound system.
  void SetGlobalParameter(plStringView sName, float fValue);

  /// \brief Returns the current value of the given global parameter.
  ///
  /// This may be different from what was set via SetGlobalParameter(), since FMOD may modify the value when playing events.
  float GetGlobalParameter(plStringView sName) const;

  void SetListenerOverrideMode(bool bEnabled);
  void SetListener(plInt32 iIndex, const plVec3& vPosition, const plVec3& vForward, const plVec3& vUp, const plVec3& vVelocity);
  plVec3 GetListenerPosition() const { return m_vListenerPosition; }

  plResult OneShotSound(plWorld* pWorld, plStringView sResourceID, const plTransform& globalPosition, float fPitch = 1.0f, float fVolume = 1.0f, bool bBlockIfNotLoaded = true);

  void RegisterTriggerMapping(plAudioSystemDataID uiId, plStringView sResourceId);
  bool TryGetTriggerMapping(plAudioSystemDataID uiId, plStringBuilder& out_sResourceId) const;

  /// Case-insensitive map from readable names (e.g. \c sonniss/footsteps/stone or \c Footstep_Stone) to a Sound Event resource id
  /// (\c bankAsset|eventPathOrGuid). Built automatically for loaded banks/events; call from game code for custom aliases.
  void RegisterTriggerReadableAlias(plStringView sReadableAlias, plStringView sFmodSoundEventResourceId);

  /// Registers hash-based ATL mappings and readable aliases for one Sound Event resource id (\c bank|pathOrGuid).
  void RegisterFmodSoundEventTriggerResolutions(plStringView sResourceId);

  /// Number of hash-based trigger mappings currently known by FMOD singleton.
  PL_NODISCARD plUInt32 GetTriggerMappingCount() const;

  /// Number of normalized readable aliases currently known by FMOD singleton.
  PL_NODISCARD plUInt32 GetReadableTriggerAliasCount() const;

  /// Maps legacy or display-only trigger strings (e.g. \c event:/Folder/Event) to a registered Sound Event resource id
  /// (\c bankAsset|eventPathOrGuid) so playback uses \c OneShotSound and the correct bank load path.
  bool TryResolveLooseTriggerToResourceId(plStringView sLooseTrigger, plStringBuilder& out_sResourceId) const;

  /// Fallback: infers \c bank|event resource id from trigger path and available bank filenames.
  bool TryInferResourceIdFromLooseTrigger(plStringView sLooseTrigger, plStringBuilder& out_sResourceId);

  /// After an FMOD bank is loaded, registers ATL trigger mappings for every event in the bank (same ids as Sound Event assets).
  /// Ensures path-based triggers resolve to \c bankResourceId|fmodPath so \c OneShotSound loads the correct bank.
  void RegisterTriggerMappingsForBank(plStringView sBankResourceId, FMOD::Studio::Bank* pBank);

private:
  friend class plFmodSoundBankResource;
  void QueueSoundBankDataForDeletion(plDataBuffer* pData);
  void ClearSoundBankDataDeletionQueue();
  mutable plMutex m_DeletionQueueMutex;

private:
  void DetectPlatform();
  plResult LoadMasterSoundBank(const char* szMasterBankResourceID);
  void LoadTriggerMap();

  bool m_bInitialized = false;
  bool m_bListenerOverrideMode = false;
  plVec3 m_vListenerPosition;
  plUInt8 m_uiNumBlendedVolumes = 4;

  FMOD::Studio::System* m_pStudioSystem;
  FMOD::System* m_pLowLevelSystem;

  struct Data
  {
    plMap<plString, float> m_VcaVolumes;
    plMap<plAudioSystemDataID, plString> m_AtlTriggerMapping;
    /// Normalized keys (trimmed, lowercased): readable trigger name -> \c bank|eventPathOrGuid
    plMap<plString, plString> m_AtlTriggerReadableToResourceId;
    /// Normalized bank-name key -> bank resource id (e.g. ?Sound/Soundbanks/Desktop/Sonniss.bank)
    plMap<plString, plString> m_AvailableBankKeyToResourceId;
    plFmodAssetProfiles m_Configs;
    plString m_sPlatform;
    plFmodSoundBankResourceHandle m_hMasterBank;
    plFmodSoundBankResourceHandle m_hMasterBankStrings;
    plHybridArray<plFmodSoundBankResourceHandle, 8> m_PreloadedBankHandles;
    plHybridArray<plDataBuffer*, 4> m_SbDeletionQueue;
    bool m_bBuiltAvailableBankLookup = false;
  };

  plUniquePtr<Data> m_pData;
};
