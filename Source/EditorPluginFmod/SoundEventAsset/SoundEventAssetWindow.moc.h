#pragma once

#include <EditorPluginFmod/SoundEventAsset/SoundEventAsset.h>
#include <Foundation/Basics.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.moc.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>

class QLabel;
class QScrollArea;

class plSoundEventAssetDocumentWindow : public plQtDocumentWindow
{
  Q_OBJECT

public:
  plSoundEventAssetDocumentWindow(plDocument* pDocument);
  ~plSoundEventAssetDocumentWindow();

  virtual const char* GetGroupName() const { return "SoundEventAsset"; }

private Q_SLOTS:


private:
  void UpdatePreview();
  void PropertyEventHandler(const plDocumentObjectPropertyEvent& e);

  plSoundEventAssetDocument* m_pAssetDoc = nullptr;
};
