#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorFramework/Assets/AssetStatusIndicator.h>
#include <EditorFramework/EngineShell/AssetDocumentContent.h>
#include <EditorFramework/EngineShell/DocumentContent.h>
#include <EditorFramework/EngineShell/DocumentFrame.h>
#include <EditorPluginFmod/EngineUi/FmodEngineViews.h>
#include <EditorPluginFmod/EngineUi/FmodShellPanels.h>
#include <EditorPluginFmod/SoundBankAsset/SoundBankAsset.h>
#include <EditorPluginFmod/SoundEventAsset/SoundEventAsset.h>
#include <GuiFoundation/DocumentWindow/DocumentWindow.h>

namespace
{
  /// The parts of a FMOD window's dock, top to bottom: parts of a fixed height keep it, the others share the rest.
  class plFmodShellPanel : public plEditorUiContent
  {
  public:
    plFmodShellPanel()
      : plEditorUiContent("FmodShellPanel")
    {
    }

    /// A child of this content; fHeight 0 takes a share of the height the fixed parts leave. Null is ignored.
    void AddPart(plEditorUiContent* pPart, float fHeight)
    {
      if (pPart == nullptr)
        return;

      m_Parts.PushBack({pPart, fHeight});
      m_vPlacedSize = plVec2(-1.0f);
    }

  protected:
    void OnUiUpdate() override
    {
      if (!IsReady())
        return;

      const plVec2 vSize = GetSurface()->GetLogicalSize();
      if (vSize == m_vPlacedSize)
        return;

      m_vPlacedSize = vSize;
      PlaceParts(vSize);
    }

  private:
    struct Part
    {
      plEditorUiContent* m_pContent = nullptr;
      float m_fHeight = 0.0f;
    };

    void PlaceParts(const plVec2& vSize)
    {
      float fFixed = 0.0f;
      plUInt32 uiShared = 0;
      for (const Part& part : m_Parts)
      {
        fFixed += part.m_fHeight;
        uiShared += part.m_fHeight > 0.0f ? 0 : 1;
      }

      const float fShared = uiShared > 0 ? plMath::Max(vSize.y - fFixed, 0.0f) / uiShared : 0.0f;

      float y = 0.0f;
      for (const Part& part : m_Parts)
      {
        const float fHeight = plMath::Min(part.m_fHeight > 0.0f ? part.m_fHeight : fShared, plMath::Max(vSize.y - y, 0.0f));
        SetChildRect(part.m_pContent, plRectFloat(0, y, vSize.x, fHeight));
        y += fHeight;
      }
    }

    plHybridArray<Part, 3> m_Parts;
    plVec2 m_vPlacedSize = plVec2(-1.0f);
  };

  void AddSoundBankParts(plEditorUiDocumentFrame& ref_frame, plFmodShellPanel& ref_panel)
  {
    // the banner and the grid, then the event:/ paths the bank provides
    auto* pDocument = static_cast<plAssetDocument*>(ref_frame.GetDocument());
    ref_panel.AddPart(ref_panel.AddChild(plUniquePtr<plEditorUiContent>(PL_DEFAULT_NEW(plEditorUiAssetProperties, pDocument, ref_frame.GetContextWindow()))), 0.0f);
    ref_panel.AddPart(ref_panel.AddChild(plUniquePtr<plEditorUiContent>(PL_DEFAULT_NEW(plFmodSubAssetsView, pDocument))), plFmodSubAssetsView::Height);
  }

  void AddSoundEventParts(plEditorUiDocumentFrame& ref_frame, plFmodShellPanel& ref_panel)
  {
    // the event has no properties: the banner above the empty state
    auto* pDocument = static_cast<plAssetDocument*>(ref_frame.GetDocument());
    ref_panel.AddPart(ref_panel.AddChild(plUniquePtr<plEditorUiContent>(PL_DEFAULT_NEW(plEditorUiAssetStatus, pDocument))), plEditorUiAssetStatus::BannerHeight);
    ref_panel.AddPart(ref_panel.AddChild(plUniquePtr<plEditorUiContent>(PL_DEFAULT_NEW(plFmodEventEmptyView, pDocument))), 0.0f);
  }

  /// The one panel of a FMOD window, under the id and title of its dock.
  class plFmodPanelProvider : public plEditorUiDocumentContentProvider
  {
  public:
    using AddPartsFunc = void (*)(plEditorUiDocumentFrame& ref_frame, plFmodShellPanel& ref_panel);

    plFmodPanelProvider(const plRTTI* pDocumentType, const char* szDock, const char* szTitle, AddPartsFunc addParts)
      : m_pDocumentType(pDocumentType)
      , m_szDock(szDock)
      , m_szTitle(szTitle)
      , m_AddParts(addParts)
    {
    }

    bool Matches(const plDocument* pDocument) const override
    {
      return pDocument->GetDynamicRTTI()->IsDerivedFrom(m_pDocumentType) && plDocumentWindow::FindWindowByDocument(pDocument) != nullptr;
    }

    void CreatePanels(plEditorUiDocumentFrame& ref_frame) const override
    {
      if (auto* pPanel = static_cast<plFmodShellPanel*>(ref_frame.AddContent({m_szDock, m_szTitle}, PL_DEFAULT_NEW(plFmodShellPanel))))
        m_AddParts(ref_frame, *pPanel);
    }

  private:
    const plRTTI* m_pDocumentType = nullptr;
    const char* m_szDock = nullptr;
    const char* m_szTitle = nullptr;
    AddPartsFunc m_AddParts = nullptr;
  };

  plFmodPanelProvider s_SoundBankProvider(plGetStaticRTTI<plSoundBankAssetDocument>(), "SoundBankAssetDockWidget", "Sound Bank Properties", &AddSoundBankParts);
  plFmodPanelProvider s_SoundEventProvider(plGetStaticRTTI<plSoundEventAssetDocument>(), "SoundEventAssetDockWidget", "Sound Event Properties", &AddSoundEventParts);
} // namespace

void plFmodShellPanels::Register()
{
  plEditorUiDocumentContentProvider::Register(&s_SoundBankProvider);
  plEditorUiDocumentContentProvider::Register(&s_SoundEventProvider);
}

void plFmodShellPanels::Unregister()
{
  plEditorUiDocumentContentProvider::Unregister(&s_SoundEventProvider);
  plEditorUiDocumentContentProvider::Unregister(&s_SoundBankProvider);
}
