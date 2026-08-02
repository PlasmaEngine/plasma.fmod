#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/SoundEventAsset/SoundEventAsset.h>

// clang-format off
PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundEventAssetDocument, 1, plRTTINoAllocator)
PL_END_DYNAMIC_REFLECTED_TYPE;

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundEventAssetProperties, 1, plRTTIDefaultAllocator<plSoundEventAssetProperties>)
PL_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

plSoundEventAssetDocument::plSoundEventAssetDocument(plStringView sDocumentPath)
  : plSimpleAssetDocument<plSoundEventAssetProperties>(sDocumentPath, plAssetDocEngineConnection::None)
{
}

void plSoundEventAssetDocument::UpdateAssetDocumentInfo(plAssetDocumentInfo* pInfo) const
{
  SUPER::UpdateAssetDocumentInfo(pInfo);
}

plTransformStatus plSoundEventAssetDocument::InternalTransformAsset(plStreamWriter& stream, plStringView sOutputTag, const plPlatformProfile* pAssetProfile,
  const plAssetFileHeader& AssetHeader, plBitflags<plTransformFlags> transformFlags)
{
  return plStatus(PL_SUCCESS);
}
