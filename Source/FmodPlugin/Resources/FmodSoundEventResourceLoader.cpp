#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodAtlData.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>
#include <FmodPlugin/Resources/FmodSoundEventResource.h>
#include <AudioSystem/Core/AudioSystem.h>
#include <AudioSystem/Core/AudioSystemAllocator.h>
#include <Foundation/IO/FileSystem/FileSystem.h>

static void BuildFriendlyTriggerName(plStringView sResourceId, plStringBuilder& out_sFriendlyName)
{
  out_sFriendlyName = sResourceId;

  // Resource IDs are "<asset-path>|<event-id-or-path>".
  if (const char* szSeparator = out_sFriendlyName.FindSubString("|"))
  {
    out_sFriendlyName = szSeparator + 1;
  }

  // Convert FMOD event paths to a clean hierarchical name for editor dropdowns.
  if (out_sFriendlyName.StartsWith_NoCase("event:/"))
  {
    out_sFriendlyName.Shrink(7, 0); // remove "event:/"
  }
  else if (out_sFriendlyName.StartsWith("/"))
  {
    out_sFriendlyName.Shrink(1, 0); // remove leading slash
  }
}

static void BuildFmodEventPath(plStringView sEventPathLike, plStringBuilder& out_sEventPath)
{
  out_sEventPath = sEventPathLike;

  if (out_sEventPath.StartsWith_NoCase("event:/"))
    return;

  if (out_sEventPath.StartsWith("/"))
  {
    out_sEventPath.Prepend("event:");
    return;
  }

  out_sEventPath.Prepend("event:/");
}

plResourceLoadData plFmodSoundEventResourceLoader::OpenDataStream(const plResource* pResource)
{
  PL_LOG_BLOCK("plFmodSoundEventResourceLoader::OpenDataStream", pResource->GetResourceID());

  LoadedData* pData = PL_DEFAULT_NEW(LoadedData);

  plResourceLoadData res;

  if (plFmod::GetSingleton()->GetStudioSystem() == nullptr)
  {
    plLog::Warning("FMOD not initialized, ignoring sound event.");
    return res;
  }

  plStringBuilder sResID;
  plFileSystem::ResolveAssetRedirection(pResource->GetResourceID(), sResID);

  const char* szSeperator = sResID.FindSubString("|");

  if (szSeperator == nullptr)
  {
    plLog::Error("FMOD event resource ID is invalid or could not be resolved: '{0}'", sResID);
    return res;
  }

  plStringBuilder sBankPath, sSubPath;
  sBankPath.SetSubString_FromTo(sResID, szSeperator);
  sSubPath = szSeperator + 1;

  pData->m_hSoundBank = plResourceManager::LoadResource<plFmodSoundBankResource>(sBankPath);
  if (!pData->m_hSoundBank.IsValid() && sBankPath.StartsWith_NoCase("Content/"))
  {
    plStringBuilder sFallbackBankPath = sBankPath;
    sFallbackBankPath.Shrink(8, 0); // strip "Content/"
    pData->m_hSoundBank = plResourceManager::LoadResource<plFmodSoundBankResource>(sFallbackBankPath);
  }

  // make sure the sound bank is fully loaded before trying to get the event descriptor (even though we go through the FMOD 'system' the
  // bank resource must be loaded first)
  {
    plResourceLock<plFmodSoundBankResource> pBank(pData->m_hSoundBank, plResourceAcquireMode::BlockTillLoaded);

    if (plConversionUtils::IsStringUuid(sSubPath))
    {
      const plUuid guid = plConversionUtils::ConvertStringToUuid(sSubPath);
      const FMOD_GUID* fmodGuid = reinterpret_cast<const FMOD_GUID*>(&guid);

      if (plFmod::GetSingleton()->GetStudioSystem()->getEventByID(fmodGuid, &pData->m_pEventDescription) != FMOD_OK)
      {
        plLog::Error("FMOD event could not be found. GUID: '{0}'", sSubPath);
        return res;
      }
    }
    else
    {
      FMOD_RESULT eventRes = plFmod::GetSingleton()->GetStudioSystem()->getEvent(sSubPath.GetData(), &pData->m_pEventDescription);
      if (eventRes != FMOD_OK || pData->m_pEventDescription == nullptr)
      {
        plStringBuilder sNormalizedPath;
        BuildFmodEventPath(sSubPath, sNormalizedPath);

        eventRes = plFmod::GetSingleton()->GetStudioSystem()->getEvent(sNormalizedPath.GetData(), &pData->m_pEventDescription);
        if (eventRes != FMOD_OK || pData->m_pEventDescription == nullptr)
        {
          plLog::Error("FMOD event could not be found. Path: '{0}'", sSubPath);
          return res;
        }
      }
    }
  }

  // make sure to load the sample data (on this thread)
  if (pData->m_pEventDescription->loadSampleData() != FMOD_OK)
  {
    plLog::Error("FMOD event sample data could not be loaded. Event: '{0}'", sSubPath);
    return res;
  }

  plFmodSoundBankResourceHandle* pHandle = &pData->m_hSoundBank;

  plMemoryStreamWriter w(&pData->m_Storage);
  w.WriteBytes(&pHandle, sizeof(plFmodSoundBankResourceHandle*)).IgnoreResult();
  w.WriteBytes(&pData->m_pEventDescription, sizeof(FMOD::Studio::EventDescription*)).IgnoreResult();

  if (auto* pAudioSystem = plAudioSystem::GetSingleton())
  {
    const plStringView sResourceId = pResource->GetResourceID();
    const plAudioSystemDataID uiResourceTriggerId = plHashHelper<plStringView>::Hash(sResourceId);
    if (pAudioSystem->GetTriggerId(sResourceId) == kInvalidAudioSystemId)
    {
      pAudioSystem->RegisterTrigger(uiResourceTriggerId, PL_AUDIOSYSTEM_NEW(plFmodAudioTriggerData, uiResourceTriggerId, sResourceId));
    }

    plFmod::GetSingleton()->RegisterFmodSoundEventTriggerResolutions(sResourceId);

    // Second ATL id: friendly path (without event:/), for selections that use short hierarchical names.
    plStringBuilder sFriendlyName;
    BuildFriendlyTriggerName(sResourceId, sFriendlyName);
    if (!sFriendlyName.IsEmpty() && !sFriendlyName.IsEqual_NoCase(sResourceId))
    {
      if (pAudioSystem->GetTriggerId(sFriendlyName.GetView()) == kInvalidAudioSystemId)
      {
        const plAudioSystemDataID uiFriendlyTriggerId = plHashHelper<plStringView>::Hash(sFriendlyName.GetView());
        pAudioSystem->RegisterTrigger(uiFriendlyTriggerId, PL_AUDIOSYSTEM_NEW(plFmodAudioTriggerData, uiFriendlyTriggerId, sResourceId));
      }
    }
  }

  res.m_pDataStream = &pData->m_Reader;
  res.m_pCustomLoaderData = pData;

  return res;
}

void plFmodSoundEventResourceLoader::CloseDataStream(const plResource* pResource, const plResourceLoadData& loaderData)
{
  LoadedData* pData = (LoadedData*)loaderData.m_pCustomLoaderData;

  PL_DEFAULT_DELETE(pData);
}

bool plFmodSoundEventResourceLoader::IsResourceOutdated(const plResource* pResource) const
{
#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)

  // if the sound bank ever gets reloaded, the sound events may be invalid, so always reload all events
  /// \todo not sure whether this can be reduced to only reloading when the bank is outdated
  return true;

#else

  return false; // we cannot reload these resources without overhead, so only allow this during development

#endif
}


