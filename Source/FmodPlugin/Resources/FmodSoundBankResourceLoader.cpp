#include <FmodPlugin/FmodPluginPCH.h>

#include <FmodPlugin/FmodIncludes.h>
#include <FmodPlugin/FmodAtlData.h>
#include <FmodPlugin/FmodSingleton.h>
#include <FmodPlugin/Resources/FmodSoundBankResource.h>
#include <AudioSystem/Core/AudioSystem.h>
#include <AudioSystem/Core/AudioSystemAllocator.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileSystem.h>
#include <Foundation/IO/OSFile.h>
#include <Foundation/Utilities/AssetFileHeader.h>

plResourceLoadData plFmodSoundBankResourceLoader::OpenDataStream(const plResource* pResource)
{
  PL_LOG_BLOCK("plFmodSoundBankResourceLoader::OpenDataStream", pResource->GetResourceID());

  LoadedData* pData = PL_DEFAULT_NEW(LoadedData);

  plResourceLoadData res;

  {
    plFileReader SoundBankAssetFile;
    if (SoundBankAssetFile.Open(pResource->GetResourceID()).Failed())
      return res;

    res.m_sResourceDescription = SoundBankAssetFile.GetFilePathRelative().GetData();
    plUInt32 uiSoundBankSize = 0;

    if (SoundBankAssetFile.GetFilePathRelative().EndsWith_NoCase("plFmodSoundBank")) // a transformed asset file
    {
      // skip the asset header
      plAssetFileHeader header;
      header.Read(SoundBankAssetFile).IgnoreResult();

      plUInt8 uiVersion = 0;
      SoundBankAssetFile >> uiVersion;

      PL_ASSERT_DEV(uiVersion == 1, "Soundbank resource file version '{0}' is invalid", uiVersion);

      SoundBankAssetFile >> uiSoundBankSize;
    }
    else
    {
      // otherwise we assume it is directly an FMOD sound bank file
      uiSoundBankSize = (plUInt32)SoundBankAssetFile.GetFileSize();
    }

    if (uiSoundBankSize > 0)
    {
      pData->m_pSoundbankData = PL_DEFAULT_NEW(plDataBuffer);
      pData->m_pSoundbankData->SetCountUninitialized(uiSoundBankSize + FMOD_STUDIO_LOAD_MEMORY_ALIGNMENT);
      plUInt8* pAlignedData = plMemoryUtils::AlignBackwards(pData->m_pSoundbankData->GetData() + FMOD_STUDIO_LOAD_MEMORY_ALIGNMENT, FMOD_STUDIO_LOAD_MEMORY_ALIGNMENT);

      SoundBankAssetFile.ReadBytes(pAlignedData, uiSoundBankSize);

      // The FMOD documentation says it is fully thread-safe, so I assume we can call loadBankMemory at any time
      auto pStudio = plFmod::GetSingleton()->GetStudioSystem();

      // this happens when FMOD is not properly configured
      if (pStudio == nullptr)
        return res;

      auto fmodRes = pStudio->loadBankMemory((const char*)pAlignedData, (int)uiSoundBankSize, FMOD_STUDIO_LOAD_MEMORY_POINT, FMOD_STUDIO_LOAD_BANK_NORMAL, &pData->m_pSoundBank);

      // if this fails with res == FMOD_ERR_NOTREADY, that might be because two processes using FMOD are running and both have the
      // FMOD_STUDIO_INIT_LIVEUPDATE flag set somehow FMOD cannot handle this and bank loading then fails
      if (fmodRes != FMOD_OK)
      {
        // if this fails with FMOD_ERR_EVENT_ALREADY_LOADED we might have attempted to load the bank multiple times in parallel
        // this is not a problem, we just discard the failed second attempt
        if (fmodRes != FMOD_ERR_EVENT_ALREADY_LOADED)
        {
          plLog::Error("Error '{1}' loading FMOD sound bank '{0}'", SoundBankAssetFile.GetFilePathRelative().GetData(), (plInt32)fmodRes);
        }

        PL_DEFAULT_DELETE(pData->m_pSoundbankData);
        PL_DEFAULT_DELETE(pData);

        res.m_pCustomLoaderData = nullptr;
        res.m_pDataStream = nullptr;
        return res;
      }
    }

#if PL_ENABLED(PL_SUPPORTS_FILE_STATS)
    {
      plFileStats stat;
      if (plFileSystem::GetFileStats(pResource->GetResourceID(), stat).Succeeded())
      {
        res.m_LoadedFileModificationDate = stat.m_LastModificationTime;
      }
    }
#endif
  }

  plMemoryStreamWriter w(&pData->m_Storage);

  w.WriteBytes(&pData->m_pSoundBank, sizeof(FMOD::Studio::Bank*)).IgnoreResult();
  w.WriteBytes(&pData->m_pSoundbankData, sizeof(plDataBuffer*)).IgnoreResult();

  res.m_pDataStream = &pData->m_Reader;
  res.m_pCustomLoaderData = pData;

  if (auto* pAudioSystem = plAudioSystem::GetSingleton())
  {
    const plAudioSystemDataID uiBankId = plHashHelper<plStringView>::Hash(pResource->GetResourceID());
    pAudioSystem->RegisterSoundBank(uiBankId, PL_AUDIOSYSTEM_NEW(plFmodAudioBankData, uiBankId, pResource->GetResourceID()));
  }

  return res;
}

void plFmodSoundBankResourceLoader::CloseDataStream(const plResource* pResource, const plResourceLoadData& loaderData)
{
  LoadedData* pData = (LoadedData*)loaderData.m_pCustomLoaderData;

  PL_DEFAULT_DELETE(pData);
}

bool plFmodSoundBankResourceLoader::IsResourceOutdated(const plResource* pResource) const
{
#if PL_ENABLED(PL_COMPILE_FOR_DEVELOPMENT)

  // don't try to reload a file that cannot be found
  plStringBuilder sAbs;
  if (plFileSystem::ResolvePath(pResource->GetResourceID(), &sAbs, nullptr).Failed())
    return false;

#  if PL_ENABLED(PL_SUPPORTS_FILE_STATS)
  if (pResource->GetLoadedFileModificationTime().IsValid())
  {
    plFileStats stat;
    if (plFileSystem::GetFileStats(pResource->GetResourceID(), stat).Failed())
      return false;

    return !stat.m_LastModificationTime.Compare(pResource->GetLoadedFileModificationTime(), plTimestamp::CompareMode::FileTimeEqual);
  }

#  endif

  return true;

#else

  return false; // we cannot reload these resources without overhead, so only allow this during development

#endif
}


