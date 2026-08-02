#pragma once

#include <EditorFramework/Assets/AssetDocumentManager.h>
#include <Foundation/Types/Status.h>

class plSimpleFmod;

class plSoundBankAssetDocumentManager : public plAssetDocumentManager
{
  PL_ADD_DYNAMIC_REFLECTION(plSoundBankAssetDocumentManager, plAssetDocumentManager);

public:
  plSoundBankAssetDocumentManager();
  ~plSoundBankAssetDocumentManager();

  virtual OutputReliability GetAssetTypeOutputReliability() const override { return plAssetDocumentManager::OutputReliability::Perfect; }

  virtual void FillOutSubAssetList(const plAssetDocumentInfo& assetInfo, plDynamicArray<plSubAssetData>& out_subAssets) const override;
  virtual plString GetAssetTableEntry(const plSubAsset* pSubAsset, plStringView sDataDirectory, const plPlatformProfile* pAssetProfile) const override;

private:
  void OnDocumentManagerEvent(const plDocumentManager::Event& e);
  plString GetSoundBankAssetTableEntry(const plSubAsset* pSubAsset, plStringView sDataDirectory, const plPlatformProfile* pAssetProfile) const;

  virtual void InternalCreateDocument(plStringView sDocumentTypeName, plStringView sPath, bool bCreateNewDocument, plDocument*& out_pDocument, const plDocumentObject* pOpenContext) override;
  virtual void InternalGetSupportedDocumentTypes(plDynamicArray<const plDocumentTypeDescriptor*>& inout_DocumentTypes) const override;

  virtual bool GeneratesProfileSpecificAssets() const override { return true; }

  virtual plUInt64 ComputeAssetProfileHashImpl(const plPlatformProfile* pAssetProfile) const override;

  plAssetDocumentTypeDescriptor m_DocTypeDesc;
  plUniquePtr<plSimpleFmod> m_pFmod;

  struct SoundBankCache
  {
    plTimestamp m_LastModification;
    plDynamicArray<plSubAssetData> m_CachedSubAssets;
  };

  mutable plMap<plUuid, SoundBankCache> m_Cache;
};
