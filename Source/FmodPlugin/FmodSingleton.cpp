#include <FmodPlugin/FmodPluginPCH.h>

#include <Core/ResourceManager/ResourceManager.h>
#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>
#include <Foundation/Configuration/CVar.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/OpenDdlReader.h>
#include <Foundation/Platform/PlatformDesc.h>
#include <Foundation/Logging/Log.h>
#include <Foundation/Strings/String.h>
#include <Foundation/Strings/StringBuilder.h>
#include <Foundation/Strings/StringUtils.h>

PL_IMPLEMENT_SINGLETON(plFmod);

static plFmod g_FmodSingleton;

#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT) && PL_ENABLED(PL_PLATFORM_WINDOWS_DESKTOP)
#include <Foundation/Basics/Platform/Win/IncludeWindows.h>
HANDLE g_hLiveUpdateMutex = NULL;
#endif

plCVarFloat cvar_FmodMasterVolume("FMOD.MasterVolume", 1.0f, plCVarFlags::Save, "Overall volume for all FMOD output");
plCVarBool cvar_FmodMute("FMOD.Mute", false, plCVarFlags::Default, "Whether FMOD output is muted");
plCVarBool cvar_FmodPause("FMOD.Pause", false, plCVarFlags::Default, "Whether FMOD output is paused");
static constexpr plStringView s_sAudioTriggerMapFile = ":project/RuntimeConfigs/AudioTriggerMap.ddl"_plsv;

plFmod::plFmod()
  : m_SingletonRegistrar(this)
{
  m_bInitialized = false;
  m_vListenerPosition.SetZero();

  m_pStudioSystem = nullptr;
  m_pLowLevelSystem = nullptr;
}

void plFmod::Startup()
{
  if (m_bInitialized)
    return;

  m_pData = PL_DEFAULT_NEW(Data);

  DetectPlatform();

  if (m_pData->m_Configs.m_AssetProfiles.IsEmpty())
  {
    LoadConfiguration(plFmodAssetProfiles::s_sConfigFile);

    if (m_pData->m_Configs.m_AssetProfiles.IsEmpty())
    {
      plLog::Warning("No valid FMOD configuration file available in '{0}'. FMOD will be deactivated.", plFmodAssetProfiles::s_sConfigFile);
      return;
    }
  }

  if (!m_pData->m_Configs.m_AssetProfiles.Find(m_pData->m_sPlatform).IsValid())
  {
    plLog::Error("FMOD configuration for platform '{0}' not available. FMOD will be deactivated.", m_pData->m_sPlatform);
    return;
  }

  const auto& config = m_pData->m_Configs.m_AssetProfiles[m_pData->m_sPlatform];

  FMOD_SPEAKERMODE fmodMode = FMOD_SPEAKERMODE_5POINT1;
  {
    plString sMode = "Unknown";
    switch (config.m_SpeakerMode)
    {
      case plFmodSpeakerMode::ModeStereo:
        sMode = "Stereo";
        fmodMode = FMOD_SPEAKERMODE_STEREO;
        break;
      case plFmodSpeakerMode::Mode5Point1:
        sMode = "5.1";
        fmodMode = FMOD_SPEAKERMODE_5POINT1;
        break;
      case plFmodSpeakerMode::Mode7Point1:
        sMode = "7.1";
        fmodMode = FMOD_SPEAKERMODE_7POINT1;
        break;
    }

    PL_LOG_BLOCK("FMOD Configuration");
    plLog::Dev("Platform = '{0}', Mode = {1}, Channels = {2}, SamplerRate = {3}", m_pData->m_sPlatform, sMode, config.m_uiVirtualChannels, config.m_uiSamplerRate);
    plLog::Dev("Master Bank = '{0}'", config.m_sMasterSoundBank);
  }

  PL_FMOD_ASSERT(FMOD::Studio::System::create(&m_pStudioSystem));

  // The example Studio project is authored for 5.1 sound, so set up the system output mode to match
  PL_FMOD_ASSERT(m_pStudioSystem->getCoreSystem(&m_pLowLevelSystem));
  PL_FMOD_ASSERT(m_pLowLevelSystem->setSoftwareFormat(config.m_uiSamplerRate, fmodMode, 0));

  void* extraDriverData = nullptr;
  FMOD_STUDIO_INITFLAGS studioflags = FMOD_STUDIO_INIT_NORMAL;

  // FMOD live update doesn't work with multiple instances and the same default IP
  // bank loading fails, once two processes are running that use this feature with the same IP
  // this could be reconfigured through the advanced settings, but for now we just enable live update for the first process
#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
  {
#  if PL_ENABLED(PL_PLATFORM_WINDOWS_DESKTOP)
    // mutex handle will be closed automatically on process termination
    GetLastError(); // clear any pending error codes
    g_hLiveUpdateMutex = CreateMutexW(nullptr, TRUE, L"plFmodLiveUpdate");

    DWORD err = GetLastError();
    if (g_hLiveUpdateMutex != NULL && err != ERROR_ALREADY_EXISTS)
    {
      studioflags |= FMOD_STUDIO_INIT_LIVEUPDATE;
    }
    else
    {
      plLog::Warning("FMOD Live-Update not available for this process, another process using FMOD is already running.");
      if (g_hLiveUpdateMutex != NULL)
      {
        CloseHandle(g_hLiveUpdateMutex); // we didn't create it, so don't keep it alive
        g_hLiveUpdateMutex = NULL;
      }
    }
#  endif
  }
#endif

  PL_FMOD_ASSERT(m_pStudioSystem->initialize(config.m_uiVirtualChannels, studioflags, FMOD_INIT_NORMAL, extraDriverData));

  if ((studioflags & FMOD_STUDIO_INIT_LIVEUPDATE) != 0)
  {
    plLog::Success("FMOD Live-Update is enabled for this process.");
  }

  if (LoadMasterSoundBank(config.m_sMasterSoundBank).Failed())
  {
    plLog::Error("Failed to load FMOD master sound bank '{0}'. Sounds will not play.", config.m_sMasterSoundBank);
    return;
  }

  LoadTriggerMap();

  m_bInitialized = true;

  UpdateSound();
}

void plFmod::Shutdown()
{
  if (m_bInitialized)
  {
    // delete all FMOD resources, except the master bank
    plResourceManager::FreeAllUnusedResources();

    m_bInitialized = false;
    m_pData->m_hMasterBank.Invalidate();
    m_pData->m_hMasterBankStrings.Invalidate();

    // now also delete the master bank
    plResourceManager::FreeAllUnusedResources();

    // now actually delete the sound bank data
    ClearSoundBankDataDeletionQueue();

    if (m_pStudioSystem != nullptr)
    {
      m_pStudioSystem->release();
      m_pStudioSystem = nullptr;
      m_pLowLevelSystem = nullptr;
    }
  }

  // finally delete all data
  m_pData.Clear();
}

void plFmod::SetNumListeners(plUInt8 uiNumListeners)
{
  PL_ASSERT_DEV(uiNumListeners <= FMOD_MAX_LISTENERS, "FMOD supports only up to {0} listeners.", FMOD_MAX_LISTENERS);

  m_pStudioSystem->setNumListeners(uiNumListeners);
}

plUInt8 plFmod::GetNumListeners() const
{
  int i = 0;
  m_pStudioSystem->getNumListeners(&i);
  return static_cast<plUInt8>(i);
}

void plFmod::LoadConfiguration(plStringView sFile)
{
  m_pData->m_Configs.Load(sFile).IgnoreResult();
}

void plFmod::SetOverridePlatform(plStringView sPlatform)
{
  m_pData->m_sPlatform = sPlatform;
}

void plFmod::UpdateSound()
{
  if (m_pStudioSystem == nullptr)
    return;

  PL_ASSERT_DEV(m_pData != nullptr, "UpdateSound() should not be called at this time.");

  // make sure to reload the sound bank, if it has been unloaded
  if (m_pData->m_hMasterBank.IsValid())
  {
    plResourceLock<plFmodSoundBankResource> pMaster(m_pData->m_hMasterBank, plResourceAcquireMode::BlockTillLoaded);
  }

  // Master Volume
  {
    FMOD::ChannelGroup* channel;
    m_pLowLevelSystem->getMasterChannelGroup(&channel);
    channel->setVolume(plMath::Clamp<float>(cvar_FmodMasterVolume, 0.0f, 1.0f));
  }

  // Mute
  {
    FMOD::ChannelGroup* channel;
    m_pLowLevelSystem->getMasterChannelGroup(&channel);

    channel->setMute(cvar_FmodMute);
  }

  // Pause
  {
    FMOD::ChannelGroup* channel;
    m_pLowLevelSystem->getMasterChannelGroup(&channel);

    channel->setPaused(cvar_FmodPause);
  }

  m_pStudioSystem->update();

  ClearSoundBankDataDeletionQueue();
}

void plFmod::SetMasterChannelVolume(float fVolume)
{
  cvar_FmodMasterVolume = plMath::Clamp<float>(fVolume, 0.0f, 1.0f);
}

float plFmod::GetMasterChannelVolume() const
{
  return cvar_FmodMasterVolume;
}

void plFmod::SetMasterChannelMute(bool bMute)
{
  cvar_FmodMute = bMute;
}

bool plFmod::GetMasterChannelMute() const
{
  return cvar_FmodMute;
}

void plFmod::SetMasterChannelPaused(bool bPaused)
{
  cvar_FmodPause = bPaused;
}

bool plFmod::GetMasterChannelPaused() const
{
  return cvar_FmodPause;
}

void plFmod::SetSoundGroupVolume(plStringView sVcaGroupGuid, float fVolume)
{
  m_pData->m_VcaVolumes[sVcaGroupGuid] = plMath::Clamp(fVolume, 0.0f, 1.0f);

  UpdateSoundGroupVolumes();
}

float plFmod::GetSoundGroupVolume(plStringView sVcaGroupGuid) const
{
  return m_pData->m_VcaVolumes.GetValueOrDefault(sVcaGroupGuid, 1.0f);
}

void plFmod::UpdateSoundGroupVolumes()
{
  for (auto it = m_pData->m_VcaVolumes.GetIterator(); it.IsValid(); ++it)
  {
    FMOD::Studio::VCA* pVca = nullptr;
    m_pStudioSystem->getVCA(it.Key().GetData(), &pVca);

    if (pVca != nullptr)
    {
      pVca->setVolume(it.Value());
    }
  }
}

void plFmod::SetNumBlendedReverbVolumes(plUInt8 uiNumBlendedVolumes)
{
  m_uiNumBlendedVolumes = plMath::Clamp<plUInt8>(uiNumBlendedVolumes, 0, 4);
}

void plFmod::SetGlobalParameter(plStringView sName, float fValue)
{
  if (m_pStudioSystem == nullptr)
    return;

  plStringBuilder sTemp(sName);
  m_pStudioSystem->setParameterByName(sTemp.GetData(), fValue);
}

float plFmod::GetGlobalParameter(plStringView sName) const
{
  if (m_pStudioSystem == nullptr)
    return 0.0f;

  plStringBuilder sTemp(sName);
  float fValue = 0.0f;
  float fFinalValue = 0.0f;
  m_pStudioSystem->getParameterByName(sTemp.GetData(), &fValue, &fFinalValue);
  return fFinalValue;
}

void plFmod::SetListenerOverrideMode(bool bEnabled)
{
  m_bListenerOverrideMode = bEnabled;
}

void plFmod::SetListener(plInt32 iIndex, const plVec3& vPosition, const plVec3& vForward, const plVec3& vUp, const plVec3& vVelocity)
{
  if (m_bListenerOverrideMode)
  {
    if (iIndex != -1)
      return;

    iIndex = 0;
  }

  if (iIndex < 0 || iIndex >= FMOD_MAX_LISTENERS)
    return;

  if (iIndex == 0)
  {
    m_vListenerPosition = vPosition;
  }

  FMOD_3D_ATTRIBUTES attr;
  attr.position.x = vPosition.x;
  attr.position.y = vPosition.y;
  attr.position.z = vPosition.z;
  attr.forward.x = vForward.x;
  attr.forward.y = vForward.y;
  attr.forward.z = vForward.z;
  attr.up.x = vUp.x;
  attr.up.y = vUp.y;
  attr.up.z = vUp.z;
  attr.velocity.x = vVelocity.x;
  attr.velocity.y = vVelocity.y;
  attr.velocity.z = vVelocity.z;

  if (m_pStudioSystem != nullptr)
  {
    m_pStudioSystem->setListenerAttributes(iIndex, &attr);
  }
}

plResult plFmod::OneShotSound(plWorld* pWorld, plStringView sResourceID, const plTransform& globalPosition, float fPitch /*= 1.0f*/, float fVolume /*= 1.0f*/, bool bBlockIfNotLoaded /*= true*/)
{
  plFmodSoundEventResourceHandle hSound = plResourceManager::LoadResource<plFmodSoundEventResource>(sResourceID);

  if (!hSound.IsValid())
    return PL_FAILURE;

  plResourceLock<plFmodSoundEventResource> pSound(hSound, bBlockIfNotLoaded ? plResourceAcquireMode::BlockTillLoaded_NeverFail : plResourceAcquireMode::AllowLoadingFallback_NeverFail);

  if (pSound.GetAcquireResult() != plResourceAcquireResult::Final)
    return PL_FAILURE;

  return pSound->PlayOnce(globalPosition, fPitch, fVolume);
}

void plFmod::DetectPlatform()
{
  if (!m_pData->m_sPlatform.IsEmpty())
    return;

#if PL_ENABLED(PL_PLATFORM_WINDOWS_DESKTOP)
  m_pData->m_sPlatform = "Desktop";

#elif PL_ENABLED(PL_PLATFORM_WINDOWS_UWP)
  m_pData->m_sPlatform = "Desktop"; /// \todo Need to detect mobile device mode

#elif PL_ENABLED(PL_PLATFORM_LINUX)
  m_pData->m_sPlatform = "Desktop";

#elif PL_ENABLED(PL_PLATFORM_OSX)
  m_pData->m_sPlatform = "Desktop";

#elif PL_ENABLED(PL_PLATFORM_ANDROID)
  m_pData->m_sPlatform = "Mobile";

#elif PL_ENABLED(PL_PLATFORM_IOS)
  m_pData->m_sPlatform = "Mobile";

#elif
#  error "Unknown Platform"
#endif
}

plResult plFmod::LoadMasterSoundBank(const char* szMasterBankResourceID)
{
  if (plStringUtils::IsNullOrEmpty(szMasterBankResourceID))
  {
    plLog::Error("FMOD master bank name has not been configured.");
    return PL_FAILURE;
  }

  m_pData->m_hMasterBank = plResourceManager::LoadResource<plFmodSoundBankResource>(szMasterBankResourceID);

  {
    plResourceLock<plFmodSoundBankResource> pResource(m_pData->m_hMasterBank, plResourceAcquireMode::BlockTillLoaded);

    if (pResource.GetAcquireResult() == plResourceAcquireResult::MissingFallback)
      return PL_FAILURE;
  }

#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
  plStringBuilder sStringsBankPath = szMasterBankResourceID;
  sStringsBankPath.ChangeFileExtension("strings.bank");

  m_pData->m_hMasterBankStrings = plResourceManager::LoadResource<plFmodSoundBankResource>(sStringsBankPath);

  {
    plResourceLock<plFmodSoundBankResource> pResource(m_pData->m_hMasterBankStrings, plResourceAcquireMode::BlockTillLoaded);

    if (pResource.GetAcquireResult() == plResourceAcquireResult::MissingFallback)
      return PL_FAILURE;
  }
#endif

  return PL_SUCCESS;
}

void plFmod::LoadTriggerMap()
{
  if (m_pData == nullptr)
    return;

  plFileReader file;
  if (file.Open(s_sAudioTriggerMapFile).Failed())
  {
    plLog::Warning("FMOD trigger map file '{0}' not found. Trigger lookups will fall back to runtime registration only.", s_sAudioTriggerMapFile);
    return;
  }

  plOpenDdlReader ddl;
  if (ddl.ParseDocument(file).Failed())
  {
    plLog::Warning("FMOD trigger map file '{0}' is invalid and was ignored.", s_sAudioTriggerMapFile);
    return;
  }

  const plOpenDdlReaderElement* pMapRoot = nullptr;
  for (const plOpenDdlReaderElement* pNode = ddl.GetRootElement()->GetFirstChild(); pNode != nullptr; pNode = pNode->GetSibling())
  {
    if (pNode->IsCustomType("AudioTriggerMap"))
    {
      pMapRoot = pNode;
      break;
    }
  }

  if (pMapRoot == nullptr)
  {
    plLog::Warning("FMOD trigger map file '{0}' does not contain an 'AudioTriggerMap' root object.", s_sAudioTriggerMapFile);
    return;
  }

  plUInt32 uiTriggerEntries = 0;
  plUInt32 uiBankEntries = 0;
  for (const plOpenDdlReaderElement* pEntry = pMapRoot->GetFirstChild(); pEntry != nullptr; pEntry = pEntry->GetSibling())
  {
    if (pEntry->IsCustomType("Trigger"))
    {
      const plOpenDdlReaderElement* pName = pEntry->FindChildOfType(plOpenDdlPrimitiveType::String, "Name");
      const plOpenDdlReaderElement* pResource = pEntry->FindChildOfType(plOpenDdlPrimitiveType::String, "Resource");
      if (pName == nullptr || pResource == nullptr)
        continue;

      const plStringView sName = pName->GetPrimitivesString()[0];
      const plStringView sResourceId = pResource->GetPrimitivesString()[0];
      if (sName.IsEmpty() || sResourceId.IsEmpty())
        continue;

      RegisterTriggerReadableAlias(sName, sResourceId);
      RegisterTriggerMapping(plHashHelper<plStringView>::Hash(sName), sResourceId);
      RegisterFmodSoundEventTriggerResolutions(sResourceId);
      ++uiTriggerEntries;
      continue;
    }

    if (pEntry->IsCustomType("Bank"))
    {
      const plOpenDdlReaderElement* pPath = pEntry->FindChildOfType(plOpenDdlPrimitiveType::String, "Path");
      if (pPath == nullptr)
        continue;

      const plStringView sBankPath = pPath->GetPrimitivesString()[0];
      if (sBankPath.IsEmpty())
        continue;

      plFmodSoundBankResourceHandle hBank = plResourceManager::LoadResource<plFmodSoundBankResource>(sBankPath);
      if (hBank.IsValid())
      {
        m_pData->m_PreloadedBankHandles.PushBack(hBank);
        ++uiBankEntries;
      }
    }
  }

  if (uiTriggerEntries == 0)
  {
    plLog::Warning("FMOD trigger map '{0}' loaded with 0 trigger entries.", s_sAudioTriggerMapFile);
  }
  else
  {
    plLog::Info("FMOD trigger map loaded: {0} trigger entries, {1} bank preloads.", uiTriggerEntries, uiBankEntries);
  }
}

void plFmod::QueueSoundBankDataForDeletion(plDataBuffer* pData)
{
  PL_LOCK(m_DeletionQueueMutex);

  // FMOD has shut down and released its studio system, so nothing reads the data any more. Banks still loaded then are
  // unloaded by the resource manager's last sweep at engine shutdown, after this plugin's data is gone.
  if (m_pData == nullptr)
  {
    PL_DEFAULT_DELETE(pData);
    return;
  }

  m_pData->m_SbDeletionQueue.PushBack(pData);
}

void plFmod::ClearSoundBankDataDeletionQueue()
{
  if (m_pData->m_SbDeletionQueue.IsEmpty())
    return;

  PL_LOCK(m_DeletionQueueMutex);

  if (m_pStudioSystem != nullptr)
  {
    // make sure the data is not in use anymore
    m_pStudioSystem->flushCommands();
  }

  for (auto pData : m_pData->m_SbDeletionQueue)
  {
    PL_DEFAULT_DELETE(pData);
  }

  m_pData->m_SbDeletionQueue.Clear();
}

void plFmod::RegisterTriggerMapping(plAudioSystemDataID uiId, plStringView sResourceId)
{
  if (uiId == 0 || sResourceId.IsEmpty() || m_pData == nullptr)
    return;

  m_pData->m_AtlTriggerMapping[uiId] = sResourceId;
}

namespace
{
  static void NormalizeBankLookupKey(plStringView sIn, plStringBuilder& out_sKey)
  {
    out_sKey = sIn;
    out_sKey.Trim(" \f\n\r\t\v/\\");
    out_sKey.ToLower();
  }

  static void ExtractTriggerRootName(plStringView sLooseTrigger, plStringBuilder& out_sRoot)
  {
    out_sRoot = sLooseTrigger;
    out_sRoot.Trim(" \f\n\r\t\v");

    if (out_sRoot.StartsWith_NoCase("event:/"))
      out_sRoot.Shrink(7, 0);
    else if (out_sRoot.StartsWith_NoCase("event:"))
      out_sRoot.Shrink(6, 0);

    out_sRoot.Trim(" \f\n\r\t\v/\\");

    if (const char* szSlash = out_sRoot.FindSubString("/"))
    {
      out_sRoot.SetSubString_FromTo(out_sRoot.GetData(), szSlash);
    }
  }

  static void BuildFriendlyTriggerNameForBank(plStringView sResourceId, plStringBuilder& out_sFriendlyName)
  {
    out_sFriendlyName = sResourceId;

    if (const char* szSeparator = out_sFriendlyName.FindSubString("|"))
    {
      out_sFriendlyName = szSeparator + 1;
    }

    if (out_sFriendlyName.StartsWith_NoCase("event:/"))
    {
      out_sFriendlyName.Shrink(7, 0);
    }
    else if (out_sFriendlyName.StartsWith("/"))
    {
      out_sFriendlyName.Shrink(1, 0);
    }
  }
} // namespace

bool plFmod::TryInferResourceIdFromLooseTrigger(plStringView sLooseTrigger, plStringBuilder& out_sResourceId)
{
  if (m_pData == nullptr || sLooseTrigger.IsEmpty())
    return false;

  if (!m_pData->m_bBuiltAvailableBankLookup)
  {
    m_pData->m_bBuiltAvailableBankLookup = true;

    auto BuildLookup = [this](plStringView sStartPath)
    {
      plFileSystemIterator fsIt;
      plFileSystem::StartSearch(fsIt, sStartPath, plFileSystemIteratorFlags::ReportFilesRecursive);
      for (; fsIt.IsValid(); fsIt.Next())
      {
        if (!fsIt.GetStats().m_sName.EndsWith_NoCase(".bank"))
          continue;

        plStringBuilder sBankResourceId = fsIt.GetCurrentPath();
        sBankResourceId.AppendPath(fsIt.GetStats().m_sName);
        if (sBankResourceId.EndsWith_NoCase(".strings.bank"))
          continue;

        plStringBuilder sBankName = fsIt.GetStats().m_sName;
        sBankName.ChangeFileExtension("");

        plStringBuilder sBankKey;
        NormalizeBankLookupKey(sBankName.GetView(), sBankKey);
        if (sBankKey.IsEmpty())
          continue;

        if (!m_pData->m_AvailableBankKeyToResourceId.Contains(sBankKey))
        {
          m_pData->m_AvailableBankKeyToResourceId[sBankKey] = sBankResourceId;
        }
      }
    };

    BuildLookup("Sound/Soundbanks/Desktop");
    if (m_pData->m_AvailableBankKeyToResourceId.IsEmpty())
    {
      BuildLookup("Sound/Soundbanks");
    }
  }

  plStringBuilder sRootName;
  ExtractTriggerRootName(sLooseTrigger, sRootName);
  if (sRootName.IsEmpty())
    return false;

  plStringBuilder sLookupKey;
  NormalizeBankLookupKey(sRootName.GetView(), sLookupKey);
  if (sLookupKey.IsEmpty())
    return false;

  const auto it = m_pData->m_AvailableBankKeyToResourceId.Find(sLookupKey);
  if (!it.IsValid())
    return false;

  plStringBuilder sEventPath = sLooseTrigger;
  sEventPath.Trim(" \f\n\r\t\v");
  if (!sEventPath.StartsWith_NoCase("event:/"))
  {
    if (sEventPath.StartsWith("/"))
      sEventPath.Prepend("event:");
    else if (sEventPath.StartsWith_NoCase("event:"))
    {
      if (!sEventPath.StartsWith_NoCase("event:/"))
      {
        sEventPath.Shrink(6, 0);
        sEventPath.Prepend("event:/");
      }
    }
    else
      sEventPath.Prepend("event:/");
  }

  out_sResourceId = it.Value();
  out_sResourceId.Append("|", sEventPath);
  return true;
}

void plFmod::RegisterTriggerReadableAlias(plStringView sReadableAlias, plStringView sFmodSoundEventResourceId)
{
  if (m_pData == nullptr || sReadableAlias.IsEmpty() || sFmodSoundEventResourceId.IsEmpty())
    return;

  plStringBuilder keyNorm;
  keyNorm.Append(sReadableAlias);
  keyNorm.Trim();
  keyNorm.ToLower();
  if (keyNorm.IsEmpty())
    return;

  const plString sKey(keyNorm.GetView());

#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)
  if (const auto it = m_pData->m_AtlTriggerReadableToResourceId.Find(sKey); it.IsValid())
  {
    if (!it.Value().IsEqual(sFmodSoundEventResourceId))
    {
      plLog::Warning("FMOD: readable trigger '{0}' already mapped to '{1}', overwriting with '{2}'.", sReadableAlias, it.Value(), sFmodSoundEventResourceId);
    }
  }
#endif

  m_pData->m_AtlTriggerReadableToResourceId[sKey] = plString(sFmodSoundEventResourceId);
}

void plFmod::RegisterFmodSoundEventTriggerResolutions(plStringView sResourceId)
{
  if (m_pData == nullptr || sResourceId.IsEmpty() || sResourceId.FindSubString("|") == nullptr)
    return;

  const plAudioSystemDataID uiResourceTriggerId = plHashHelper<plStringView>::Hash(sResourceId);
  RegisterTriggerMapping(uiResourceTriggerId, sResourceId);

  plStringBuilder sFriendlyName;
  BuildFriendlyTriggerNameForBank(sResourceId, sFriendlyName);

  if (!sFriendlyName.IsEmpty() && !sFriendlyName.IsEqual_NoCase(sResourceId))
  {
    const plAudioSystemDataID uiFriendlyTriggerId = plHashHelper<plStringView>::Hash(sFriendlyName.GetView());
    RegisterTriggerMapping(uiFriendlyTriggerId, sResourceId);
  }

  plStringBuilder sEventPath;
  if (const char* szSep = sResourceId.FindSubString("|"))
  {
    sEventPath = szSep + 1;
  }

  if (!sEventPath.IsEmpty())
  {
    RegisterTriggerReadableAlias(sEventPath.GetView(), sResourceId);
  }

  if (!sFriendlyName.IsEmpty())
  {
    RegisterTriggerReadableAlias(sFriendlyName.GetView(), sResourceId);
  }
}

void plFmod::RegisterTriggerMappingsForBank(plStringView sBankResourceId, FMOD::Studio::Bank* pBank)
{
  if (m_pData == nullptr || pBank == nullptr || sBankResourceId.IsEmpty())
    return;

  int iEventCount = 0;
  if (pBank->getEventCount(&iEventCount) != FMOD_OK || iEventCount <= 0)
    return;

  plDynamicArray<FMOD::Studio::EventDescription*> events;
  events.SetCountUninitialized(iEventCount);
  int iActualEvents = 0;
  if (pBank->getEventList(events.GetData(), iEventCount, &iActualEvents) != FMOD_OK)
    return;

  char szPath[512];

  for (int i = 0; i < iActualEvents; ++i)
  {
    FMOD::Studio::EventDescription* pDesc = events[i];
    if (pDesc == nullptr)
      continue;

    int iPathRetrieved = 0;
    if (pDesc->getPath(szPath, PL_ARRAY_SIZE(szPath), &iPathRetrieved) != FMOD_OK || iPathRetrieved <= 0)
      continue;

    plStringBuilder sResourceId;
    sResourceId.Append(sBankResourceId, "|", szPath);

    RegisterFmodSoundEventTriggerResolutions(sResourceId.GetView());
  }
}

plUInt32 plFmod::GetTriggerMappingCount() const
{
  return m_pData != nullptr ? m_pData->m_AtlTriggerMapping.GetCount() : 0;
}

plUInt32 plFmod::GetReadableTriggerAliasCount() const
{
  return m_pData != nullptr ? m_pData->m_AtlTriggerReadableToResourceId.GetCount() : 0;
}

bool plFmod::TryGetTriggerMapping(plAudioSystemDataID uiId, plStringBuilder& out_sResourceId) const
{
  if (m_pData == nullptr)
    return false;

  if (const auto it = m_pData->m_AtlTriggerMapping.Find(uiId); it.IsValid())
  {
    out_sResourceId = it.Value();
    return true;
  }

  return false;
}

bool plFmod::TryResolveLooseTriggerToResourceId(plStringView sLooseTrigger, plStringBuilder& out_sResourceId) const
{
  if (m_pData == nullptr || sLooseTrigger.IsEmpty())
    return false;

  // Always check the readable alias map first — the trigger name may be a SubAssetIdentifier
  // (e.g. "Content/Sound/Bank.plSoundBankAsset|EventName") that contains "|" but is NOT
  // a valid runtime resource ID. The trigger map maps these to proper bank|guid resource IDs.
  plStringBuilder lookupNorm;
  lookupNorm.Append(sLooseTrigger);
  lookupNorm.Trim();
  lookupNorm.ToLower();
  if (!lookupNorm.IsEmpty())
  {
    const plString sLookupKey(lookupNorm.GetView());
    if (const auto it = m_pData->m_AtlTriggerReadableToResourceId.Find(sLookupKey); it.IsValid())
    {
      out_sResourceId = it.Value();
      return true;
    }
  }

  if (TryGetTriggerMapping(plHashHelper<plStringView>::Hash(sLooseTrigger), out_sResourceId))
    return true;

  if (!lookupNorm.IsEmpty() && !lookupNorm.IsEqual(sLooseTrigger))
  {
    if (TryGetTriggerMapping(plHashHelper<plStringView>::Hash(lookupNorm.GetView()), out_sResourceId))
      return true;
  }

  // Last resort: if it already contains "|" assume it is a direct resource ID.
  if (sLooseTrigger.FindSubString("|") != nullptr)
  {
    out_sResourceId = sLooseTrigger;
    return true;
  }

  return false;
}

PL_STATICLINK_FILE(FmodPlugin, FmodPlugin_FmodSingleton);
