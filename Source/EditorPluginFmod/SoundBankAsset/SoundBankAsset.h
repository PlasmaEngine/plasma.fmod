#pragma once

#include <EditorFramework/Assets/SimpleAssetDocument.h>

class plSoundBankAssetProperties : public plReflectedClass
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundBankAssetProperties, plReflectedClass);

public:
  plSoundBankAssetProperties() = default;

  plString m_sSoundBank;
};

class plSoundBankAssetDocument : public plSimpleAssetDocument<plSoundBankAssetProperties>
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundBankAssetDocument, plSimpleAssetDocument<plSoundBankAssetProperties>);

public:
  plSoundBankAssetDocument(plStringView sDocumentPath);

protected:
  virtual void UpdateAssetDocumentInfo(plAssetDocumentInfo* pInfo) const override;
  virtual plTransformStatus InternalTransformAsset(plStreamWriter& stream, plStringView sOutputTag, const plPlatformProfile* pAssetProfile,
    const plAssetFileHeader& AssetHeader, plBitflags<plTransformFlags> transformFlags) override;
};
