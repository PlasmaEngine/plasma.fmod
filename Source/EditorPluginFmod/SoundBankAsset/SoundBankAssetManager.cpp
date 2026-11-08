#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorPluginFmod/SoundBankAsset/SoundBankAssetManager.h>
#include <EditorPluginFmod/SoundBankAsset/SoundBankAssetWindow.h>
#include <FmodPlugin/FmodIncludes.h>
#include <Foundation/IO/OSFile.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundBankAssetDocumentManager, 1, plRTTIDefaultAllocator<plSoundBankAssetDocumentManager>)
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

class plSimpleFmod
{
public:
  plSimpleFmod() = default;
  ~plSimpleFmod() { PL_ASSERT_DEV(m_pSystem == nullptr, "FMod is not shut down"); }

  void Startup()
  {
    PL_ASSERT_DEV(m_pSystem == nullptr, "FMod is not shut down");

    PL_FMOD_ASSERT(FMOD::Studio::System::create(&m_pSystem));

    void* extraDriverData = nullptr;
    PL_FMOD_ASSERT(m_pSystem->initialize(32, FMOD_STUDIO_INIT_NORMAL, FMOD_INIT_NORMAL, extraDriverData));
  }

  void Shutdown()
  {
    if (m_pSystem == nullptr)
      return;

    PL_FMOD_ASSERT(m_pSystem->unloadAll());
    PL_FMOD_ASSERT(m_pSystem->release());

    m_pSystem = nullptr;
  }

  FMOD::Studio::System* GetSystem()
  {
    if (m_pSystem == nullptr)
      Startup();

    return m_pSystem;
  }

private:
  FMOD::Studio::System* m_pSystem = nullptr;
};


plSoundBankAssetDocumentManager::plSoundBankAssetDocumentManager()
{
  plDocumentManager::s_Events.AddEventHandler(plMakeDelegate(&plSoundBankAssetDocumentManager::OnDocumentManagerEvent, this));

  m_DocTypeDesc.m_sDocumentTypeName = "Sound Bank";
  m_DocTypeDesc.m_sFileExtension = "plSoundBankAsset";
  m_DocTypeDesc.m_sIcon = ":/AssetIcons/Sound_Bank.svg";
  m_DocTypeDesc.m_sAssetCategory = "Audio";
  m_DocTypeDesc.m_pDocumentType = plGetStaticRTTI<plSoundBankAssetDocument>();
  m_DocTypeDesc.m_pManager = this;
  m_DocTypeDesc.m_CompatibleTypes.PushBack("CompatibleAsset_Fmod_Bank");

  m_DocTypeDesc.m_sResourceFileExtension = "plBinFmodSoundBank";
  m_DocTypeDesc.m_AssetDocumentFlags = plAssetDocumentFlags::None;

  m_pFmod = PL_DEFAULT_NEW(plSimpleFmod);
}

plSoundBankAssetDocumentManager::~plSoundBankAssetDocumentManager()
{
  plDocumentManager::s_Events.RemoveEventHandler(plMakeDelegate(&plSoundBankAssetDocumentManager::OnDocumentManagerEvent, this));

  m_pFmod->Shutdown();
  m_pFmod.Clear();
}

void plSoundBankAssetDocumentManager::FillOutSubAssetList(const plAssetDocumentInfo& assetInfo, plDynamicArray<plSubAssetData>& out_subAssets) const
{
  PL_PROFILE_SCOPE("GetSoundBankSubAssets");

  SoundBankCache& cache = m_Cache[assetInfo.m_DocumentID];
  const plTimestamp lastTS = cache.m_LastModification;
  bool bCanEarlyOut = true;

  for (const plString& dep : assetInfo.m_TransformDependencies)
  {
    if (!plPathUtils::HasExtension(dep, "bank"))
      continue;

    {
      plString sAssetFile = dep;
      if (!plEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(sAssetFile))
        continue;

      plFileStats stat;
      if (plOSFile::GetFileStats(sAssetFile, stat).Succeeded())
      {
        if (stat.m_LastModificationTime.Compare(cache.m_LastModification, plTimestamp::CompareMode::Newer))
        {
          cache.m_LastModification = stat.m_LastModificationTime;
        }

        if (stat.m_LastModificationTime.Compare(lastTS, plTimestamp::CompareMode::Newer))
        {
          bCanEarlyOut = false;
        }
      }
    }
  }

  if (bCanEarlyOut)
  {
    out_subAssets = cache.m_CachedSubAssets;
    return;
  }

  cache.m_CachedSubAssets.Clear();

  plHashedString sAssetsDocumentTypeName;
  sAssetsDocumentTypeName.Assign("Sound Event");

  auto* pSystem = m_pFmod->GetSystem();
  plHybridArray<FMOD::Studio::Bank*, 16> loadedBanks;


  // TODO: it is unclear whether the code below can produce deadlocks, because of locked soundbank files on disk
  // in theory, since m_TransformDependencies is alphabetically sorted, the access order should always be the same, and thus
  // no deadlock should be possible,

  for (const plString& dep : assetInfo.m_TransformDependencies)
  {
    if (!plPathUtils::HasExtension(dep, "bank"))
      continue;

    plString sAssetFile = dep;
    if (!plEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(sAssetFile))
      continue;

    FMOD::Studio::Bank* pBank = nullptr;
    auto res = pSystem->loadBankFile(sAssetFile, FMOD_STUDIO_LOAD_BANK_NORMAL, &pBank);
    if (res != FMOD_OK || pBank == nullptr)
      continue;

    loadedBanks.PushBack(pBank);

    plStringBuilder sStringsBank = sAssetFile;
    sStringsBank.PathParentDirectory();
    sStringsBank.AppendPath("*.strings.bank");

    // honestly we have no idea what the strings bank name should be
    // and if there are multiple, which one is the correct one
    // so we just load everything that we can find
    plFileSystemIterator fsIt;
    for (fsIt.StartSearch(sStringsBank, plFileSystemIteratorFlags::ReportFiles); fsIt.IsValid(); fsIt.Next())
    {
      sStringsBank = fsIt.GetCurrentPath();
      sStringsBank.AppendPath(fsIt.GetStats().m_sName);

      FMOD::Studio::Bank* pStringsBank = nullptr;
      if (pSystem->loadBankFile(sStringsBank, FMOD_STUDIO_LOAD_BANK_NORMAL, &pStringsBank) == FMOD_OK && pStringsBank != nullptr)
      {
        loadedBanks.PushBack(pStringsBank);
      }
    }

    int iEvents = 0;
    PL_FMOD_ASSERT(pBank->getEventCount(&iEvents));

    if (iEvents > 0)
    {
      plDynamicArray<FMOD::Studio::EventDescription*> events;
      events.SetCountUninitialized(iEvents);

      pBank->getEventList(events.GetData(), iEvents, &iEvents);

      char szPath[256];
      int iLen;

      FMOD_GUID guid;

      plStringBuilder sGuid, sGuidNoSpace, sEventName;

      for (plUInt32 i = 0; i < events.GetCount(); ++i)
      {
        iLen = 0;
        PL_FMOD_ASSERT(events[i]->getPath(szPath, 255, &iLen));
        szPath[iLen] = '\0';

        sEventName = szPath;

        if (sEventName.StartsWith_NoCase("snapshot:/"))
          continue;

        if (sEventName.StartsWith_NoCase("event:/"))
          sEventName.Shrink(7, 0);
        else
        {
          plLog::Warning("Skipping unknown FMOD event type: '{0}", sEventName);
          continue;
        }

        events[i]->getID(&guid);

        plUuid* plGuid = reinterpret_cast<plUuid*>(&guid);
        plConversionUtils::ToString(*plGuid, sGuid);
        sGuidNoSpace = sGuid;
        sGuidNoSpace.ReplaceAll(" ", "");

        auto& sub = cache.m_CachedSubAssets.ExpandAndGetRef();
        sub.m_Guid = *plGuid;
        sub.m_sName = sEventName;
        sub.m_sSubAssetsDocumentTypeName = sAssetsDocumentTypeName;
      }
    }

    for (FMOD::Studio::Bank* pBank : loadedBanks)
    {
      PL_FMOD_ASSERT(pBank->unload());
    }

    loadedBanks.Clear();
  }

  PL_ASSERT_DEV(loadedBanks.IsEmpty(), "A soundbank wasn't unloaded.");

  out_subAssets = cache.m_CachedSubAssets;
}

plString plSoundBankAssetDocumentManager::GetSoundBankAssetTableEntry(const plSubAsset* pSubAsset, plStringView sDataDirectory, const plPlatformProfile* pAssetProfile) const
{
  // at the moment we don't reference the actual transformed asset file
  // instead we reference the source FMOD sound bank file
  // this makes development easier, as we don't need to wait for an asset transform before changes are available

  /// \todo For final release we should reference the transformed file, as it's the one that gets packaged etc.
  /// Maybe we should add another platform target for that ?

  // if (pAssetProfile == plAssetCurator::GetSingleton()->GetDevelopmentAssetProfile())
  {
    for (const plString& dep : pSubAsset->m_pAssetInfo->m_Info->m_TransformDependencies)
    {
      if (dep.EndsWith_NoCase(".bank") && !dep.EndsWith_NoCase(".strings.bank"))
      {
        plStringBuilder result;
        result.Set("?", dep); // ? is an option to tell the system to skip the redirection prefix and use the path as is
        return result;
      }
    }
  }
  // else
  //{
  //  SUPER::GetAssetTableEntry(pSubAsset, szDataDirectory, pAssetProfile);
  //}

  return plString();
}

plString plSoundBankAssetDocumentManager::GetAssetTableEntry(const plSubAsset* pSubAsset, plStringView sDataDirectory, const plPlatformProfile* pAssetProfile) const
{
  if (pSubAsset->m_bMainAsset)
  {
    return GetSoundBankAssetTableEntry(pSubAsset, sDataDirectory, pAssetProfile);
  }
  else
  {
    plStringBuilder result = GetSoundBankAssetTableEntry(pSubAsset, sDataDirectory, pAssetProfile);

    plStringBuilder sGuid;
    plConversionUtils::ToString(pSubAsset->m_Data.m_Guid, sGuid);

    result.Append("|", sGuid);

    return result;
  }
}

void plSoundBankAssetDocumentManager::OnDocumentManagerEvent(const plDocumentManager::Event& e)
{
  switch (e.m_Type)
  {
    case plDocumentManager::Event::Type::DocumentWindowRequested:
    {
      if (e.m_pDocument->GetDynamicRTTI() == plGetStaticRTTI<plSoundBankAssetDocument>())
      {
        new plSoundBankAssetDocumentWindow(e.m_pDocument); // NOLINT: Not a memory leak
      }
    }
    break;
    default:
      break;
  }
}

void plSoundBankAssetDocumentManager::InternalCreateDocument(plStringView sDocumentTypeName, plStringView sPath, bool bCreateNewDocument, plDocument*& out_pDocument, const plDocumentObject* pOpenContext)
{
  out_pDocument = new plSoundBankAssetDocument(sPath);
}

void plSoundBankAssetDocumentManager::InternalGetSupportedDocumentTypes(plDynamicArray<const plDocumentTypeDescriptor*>& inout_DocumentTypes) const
{
  inout_DocumentTypes.PushBack(&m_DocTypeDesc);
}

plUInt64 plSoundBankAssetDocumentManager::ComputeAssetProfileHashImpl(const plPlatformProfile* pAssetProfile) const
{
  // don't have any settings yet, but assets that generate profile specific output must not return 0 here
  return 1;
}
