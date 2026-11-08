#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/SoundBankAsset/SoundBankAssetWindow.h>

plSoundBankAssetDocumentWindow::plSoundBankAssetDocumentWindow(plDocument* pDocument)
  : plDocumentWindow(pDocument)
{
  SetMenuBarActions("SoundBankAssetMenuBar");
  SetToolBarActions("SoundBankAssetToolBar");

  pDocument->GetSelectionManager()->SetSelection(pDocument->GetObjectManager()->GetRootObject()->GetChildren()[0]);

  m_pAssetDoc = static_cast<plSoundBankAssetDocument*>(pDocument);
}
