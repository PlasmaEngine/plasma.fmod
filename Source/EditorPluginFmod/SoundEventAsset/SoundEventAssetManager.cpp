#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/SoundEventAsset/SoundEventAssetManager.h>
#include <EditorPluginFmod/SoundEventAsset/SoundEventAssetWindow.h>

PL_BEGIN_DYNAMIC_REFLECTED_TYPE(plSoundEventAssetDocumentManager, 1, plRTTIDefaultAllocator<plSoundEventAssetDocumentManager>)
PL_END_DYNAMIC_REFLECTED_TYPE;

plSoundEventAssetDocumentManager::plSoundEventAssetDocumentManager()
{
  plDocumentManager::s_Events.AddEventHandler(plMakeDelegate(&plSoundEventAssetDocumentManager::OnDocumentManagerEvent, this));

  m_DocTypeDesc.m_bCanCreate = false;
  m_DocTypeDesc.m_sDocumentTypeName = "Sound Event";
  m_DocTypeDesc.m_sFileExtension = "plSoundEventAsset";
  m_DocTypeDesc.m_sIcon = ":/AssetIcons/Sound_Event.svg";
  m_DocTypeDesc.m_sAssetCategory = "Audio";
  m_DocTypeDesc.m_pDocumentType = plGetStaticRTTI<plSoundEventAssetDocument>();
  m_DocTypeDesc.m_pManager = this;
  m_DocTypeDesc.m_CompatibleTypes.PushBack("CompatibleAsset_Fmod_Event");

  m_DocTypeDesc.m_sResourceFileExtension = "plBinFmodSoundEvent";
  m_DocTypeDesc.m_AssetDocumentFlags = plAssetDocumentFlags::None;
}

plSoundEventAssetDocumentManager::~plSoundEventAssetDocumentManager()
{
  plDocumentManager::s_Events.RemoveEventHandler(plMakeDelegate(&plSoundEventAssetDocumentManager::OnDocumentManagerEvent, this));
}

void plSoundEventAssetDocumentManager::OnDocumentManagerEvent(const plDocumentManager::Event& e)
{
  switch (e.m_Type)
  {
    case plDocumentManager::Event::Type::DocumentWindowRequested:
    {
      if (e.m_pDocument->GetDynamicRTTI() == plGetStaticRTTI<plSoundEventAssetDocument>())
      {
        new plSoundEventAssetDocumentWindow(e.m_pDocument); // NOLINT: Not a memory leak
      }
    }
    break;
    default:
      break;
  }
}

void plSoundEventAssetDocumentManager::InternalCreateDocument(plStringView sDocumentTypeName, plStringView sPath, bool bCreateNewDocument, plDocument*& out_pDocument, const plDocumentObject* pOpenContext)
{
  out_pDocument = new plSoundEventAssetDocument(sPath);
}

void plSoundEventAssetDocumentManager::InternalGetSupportedDocumentTypes(plDynamicArray<const plDocumentTypeDescriptor*>& inout_DocumentTypes) const
{
  inout_DocumentTypes.PushBack(&m_DocTypeDesc);
}
