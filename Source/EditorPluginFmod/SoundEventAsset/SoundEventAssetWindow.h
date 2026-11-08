#pragma once

#include <EditorPluginFmod/SoundEventAsset/SoundEventAsset.h>
#include <Foundation/Basics.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>

/// The FMOD Sound Event window: its menu bar and toolbar. Its panel (the status banner and the empty state) comes from
/// plFmodShellPanels.
class plSoundEventAssetDocumentWindow : public plDocumentWindow
{
public:
  plSoundEventAssetDocumentWindow(plDocument* pDocument);
  ~plSoundEventAssetDocumentWindow();

private:
  void UpdatePreview();
  void PropertyEventHandler(const plDocumentObjectPropertyEvent& e);

  plSoundEventAssetDocument* m_pAssetDoc = nullptr;
};
