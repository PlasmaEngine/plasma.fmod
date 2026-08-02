#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/SoundBankAsset/SoundBankAsset.h>
#include <Foundation/IO/FileSystem/FileReader.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundBankAssetDocument, 1, plRTTINoAllocator)
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundBankAssetProperties, 1, plRTTIDefaultAllocator<plSoundBankAssetProperties>)
{
  PL_BEGIN_PROPERTIES
  {
    PL_MEMBER_PROPERTY("SoundBankFile", m_sSoundBank)->AddAttributes(new plFileBrowserAttribute("Select SoundBank", "*.bank")),
  }
  PL_END_PROPERTIES;
}
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plSoundBankAssetDocument::plSoundBankAssetDocument(plStringView sDocumentPath)
  : plSimpleAssetDocument<plSoundBankAssetProperties>(sDocumentPath, plAssetDocEngineConnection::None)
{
}

void plSoundBankAssetDocument::UpdateAssetDocumentInfo(plAssetDocumentInfo* pInfo) const
{
  SUPER::UpdateAssetDocumentInfo(pInfo);

  const plSoundBankAssetProperties* pProp = GetProperties();

  pInfo->m_TransformDependencies.Insert(pProp->m_sSoundBank);
}

plTransformStatus plSoundBankAssetDocument::InternalTransformAsset(plStreamWriter& stream, plStringView sOutputTag, const plPlatformProfile* pAssetProfile, const plAssetFileHeader& AssetHeader, plBitflags<plTransformFlags> transformFlags)
{
  const plSoundBankAssetProperties* pProp = GetProperties();

  if (pProp->m_sSoundBank.IsEmpty())
    return plStatus("No sound-bank file has been specified.");

  if (!plPathUtils::HasExtension(pProp->m_sSoundBank, "bank"))
    return plStatus(plFmt("Specified sound-bank file should have 'bank' extension: '{0}'", pProp->m_sSoundBank));

  /// \todo For platform specific sound banks, adjust the path to point to the correct file

  plStringBuilder sAssetFile = pProp->m_sSoundBank;
  if (!plQtEditorApp::GetSingleton()->MakeDataDirectoryRelativePathAbsolute(sAssetFile))
    return plStatus(plFmt("Failed to make sound-bank path absolute: '{0}'", pProp->m_sSoundBank));

  plFileReader SoundBankFile;
  if (SoundBankFile.Open(sAssetFile).Failed())
    return plStatus(plFmt("Could not open sound-bank for reading: '{0}'", sAssetFile));

  // we copy the entire sound bank into our transformed asset
  // however, at least during development, we typically do not load the data from there,
  // but from the FMOD sound bank files directly, so that we do not need to wait for an asset transform

  plDefaultMemoryStreamStorage storage;

  // copy the file from disk into memory
  {
    plMemoryStreamWriter writer(&storage);

    plUInt8 Temp[4 * 1024];

    while (true)
    {
      plUInt64 uiRead = SoundBankFile.ReadBytes(Temp, PL_ARRAY_SIZE(Temp));

      if (uiRead == 0)
        break;

      writer.WriteBytes(Temp, uiRead).IgnoreResult();
    }
  }

  const plUInt8 uiVersion = 1;
  stream << uiVersion;

  // now store the entire file in our asset output
  stream << storage.GetStorageSize32();
  return storage.CopyToStream(stream);
}
