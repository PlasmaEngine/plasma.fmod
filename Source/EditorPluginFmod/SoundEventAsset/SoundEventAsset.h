#pragma once

#include <EditorFramework/Assets/SimpleAssetDocument.h>

class plSoundEventAssetProperties : public plReflectedClass
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundEventAssetProperties, plReflectedClass);

public:
  plSoundEventAssetProperties() = default;
};


class plSoundEventAssetDocument : public plSimpleAssetDocument<plSoundEventAssetProperties>
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundEventAssetDocument, plSimpleAssetDocument<plSoundEventAssetProperties>);

public:
  plSoundEventAssetDocument(plStringView sDocumentPath);

protected:
  virtual void UpdateAssetDocumentInfo(plAssetDocumentInfo* pInfo) const override;
  virtual plTransformStatus InternalTransformAsset(plStreamWriter& stream, plStringView sOutputTag, const plPlatformProfile* pAssetProfile,
    const plAssetFileHeader& AssetHeader, plBitflags<plTransformFlags> transformFlags) override;
};
