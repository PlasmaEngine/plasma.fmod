#pragma once

#include <EditorPluginFmod/SoundBankAsset/SoundBankAsset.h>
#include <Foundation/Basics.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.moc.h>
#include <ToolsFoundation/Object/DocumentObjectManager.h>

class QLabel;
class QScrollArea;
class QtImageWidget;

class plSoundBankAssetDocumentWindow : public plQtDocumentWindow
{
  Q_OBJECT

public:
  plSoundBankAssetDocumentWindow(plDocument* pDocument);

  virtual const char* GetGroupName() const { return "SoundBankAsset"; }

private:
  plSoundBankAssetDocument* m_pAssetDoc;
};
