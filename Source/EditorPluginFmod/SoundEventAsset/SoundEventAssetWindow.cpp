#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/SoundEventAsset/SoundEventAssetWindow.h>

plSoundEventAssetDocumentWindow::plSoundEventAssetDocumentWindow(plDocument* pDocument)
  : plDocumentWindow(pDocument)
{
  GetDocument()->GetObjectManager()->m_PropertyEvents.AddEventHandler(plMakeDelegate(&plSoundEventAssetDocumentWindow::PropertyEventHandler, this));

  SetMenuBarActions("SoundEventAssetMenuBar");
  SetToolBarActions("SoundEventAssetToolBar");

  pDocument->GetSelectionManager()->SetSelection(pDocument->GetObjectManager()->GetRootObject()->GetChildren()[0]);

  m_pAssetDoc = static_cast<plSoundEventAssetDocument*>(pDocument);

  UpdatePreview();
}

plSoundEventAssetDocumentWindow::~plSoundEventAssetDocumentWindow()
{
  GetDocument()->GetObjectManager()->m_PropertyEvents.RemoveEventHandler(
    plMakeDelegate(&plSoundEventAssetDocumentWindow::PropertyEventHandler, this));
}

void plSoundEventAssetDocumentWindow::UpdatePreview()
{
  const auto& prop = ((plSoundEventAssetDocument*)GetDocument())->GetProperties();

  // plStringBuilder s;
  // s.SetFormat("Vertices: {0}\nTriangles: {1}\nSubMeshes: {2}", prop->m_uiVertices, prop->m_uiTriangles, prop->m_SlotNames.GetCount());

  // for (plUInt32 m = 0; m < prop->m_SlotNames.GetCount(); ++m)
  //  s.AppendFormat("\nSlot {0}: {1}", m, prop->m_SlotNames[m]);
}

void plSoundEventAssetDocumentWindow::PropertyEventHandler(const plDocumentObjectPropertyEvent& e)
{
  // if (e.m_sPropertyPath == "Texture File")
  //{
  //  UpdatePreview();
  //}
}
