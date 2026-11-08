#pragma once

#include <EditorPluginFmod/SoundBankAsset/SoundBankAsset.h>
#include <Foundation/Basics.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>

/// The FMOD Sound Bank window: its menu bar and toolbar. Its panel (the properties and the Sub-assets list) comes from
/// plFmodShellPanels.
class plSoundBankAssetDocumentWindow : public plDocumentWindow
{
public:
  plSoundBankAssetDocumentWindow(plDocument* pDocument);

private:
  plSoundBankAssetDocument* m_pAssetDoc;
};
