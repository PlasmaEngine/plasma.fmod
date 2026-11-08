#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorPluginFmod/EngineUi/FmodEngineViews.h>
#include <EditorUi/Runtime.h>
#include <EditorUi/Surface.h>
#include <EditorUi/Widgets.h>
#include <Foundation/Math/ColorScheme.h>
#include <ToolsFoundation/Document/Document.h>

namespace plFmodEngineUi
{
  plColorGammaUB AudioColor()
  {
    return plColorGammaUB(plColorScheme::GetCategoryColor("Audio", plColorScheme::CategoryColorUsage::BorderColor));
  }
} // namespace plFmodEngineUi

namespace
{
  const plHashedString s_sCount = plMakeHashedString("Fmod.SubAssets.Count");
  const plHashedString s_sEmptyDrawing = plMakeHashedString("Fmod.Empty");
} // namespace

//////////////////////////////////////////////////////////////////////////
// plFmodSubAssetsView

/// The bank's sub-assets by name; without any one dim row.
class plFmodSubAssetsView::Adapter : public plEditorUiTreeAdapter
{
public:
  struct Entry
  {
    plString m_sName;
    plString m_sGuid;
  };

  plDynamicArray<Entry> m_Entries;

  void GetChildren(plUInt64 uiParent, plDynamicArray<plUInt64>& out_children) const override
  {
    out_children.Clear();
    if (uiParent != 0)
      return;

    for (plUInt32 i = 0; i < plMath::Max<plUInt32>(m_Entries.GetCount(), 1); ++i)
      out_children.PushBack(i + 1);
  }

  plUInt64 GetParent(plUInt64 uiItem) const override
  {
    PL_IGNORE_UNUSED(uiItem);
    return 0;
  }

  void GetRow(plUInt64 uiItem, plEditorUiTreeRow& out_row) const override
  {
    if (m_Entries.IsEmpty())
    {
      out_row.m_sText = "No sub-assets";
      out_row.m_TextStyle = plEditorUiTreeTextStyle::Dimmed;
      return;
    }

    out_row.m_sText = m_Entries[static_cast<plUInt32>(uiItem - 1)].m_sName;
    out_row.m_sTooltip = "Sound events the bank provides. Double-click to open one.";
    out_row.m_sIcon = plEditorUi::MonochromeIconPath(plFmodEngineUi::IconEvent);
    out_row.m_IconColor = plFmodEngineUi::AudioColor();
  }

  void Activate(plUInt64 uiItem) override
  {
    if (m_Entries.IsEmpty() || uiItem == 0 || uiItem > m_Entries.GetCount())
      return;

    plEditorApp::GetSingleton()->OpenDocument(m_Entries[static_cast<plUInt32>(uiItem - 1)].m_sGuid, plDocumentFlags::RequestWindow | plDocumentFlags::AddToRecentFilesList);
  }
};

plFmodSubAssetsView::plFmodSubAssetsView(plDocument* pDocument)
  : plEditorUiContent("FmodSubAssets")
  , m_pDocument(pDocument)
  , m_pAlive(std::make_shared<bool>(true))
{
  m_pAdapter = PL_DEFAULT_NEW(Adapter);

  if (IsReady())
    m_pTree = PL_DEFAULT_NEW(plEditorUiTree, *GetSurface(), "Fmod.SubAssets", *m_pAdapter);

  plAssetCurator::GetSingleton()->m_Events.AddEventHandler(plMakeDelegate(&plFmodSubAssetsView::AssetCuratorEventHandler, this));
}

plFmodSubAssetsView::~plFmodSubAssetsView()
{
  plAssetCurator::GetSingleton()->m_Events.RemoveEventHandler(plMakeDelegate(&plFmodSubAssetsView::AssetCuratorEventHandler, this));
  m_pTree.Clear();
}

void plFmodSubAssetsView::AssetCuratorEventHandler(const plAssetCuratorEvent& e)
{
  PL_IGNORE_UNUSED(e);

  // curator events can come from other threads; the list is read again on the main thread, while the view lives
  std::weak_ptr<bool> alive = m_pAlive;
  plEditorUiRuntime::RunAfterFrame([alive, this]()
    {
      if (alive.expired())
        return;
      m_bRefresh = true;
      RequestRedraw(); });
}

void plFmodSubAssetsView::Refresh()
{
  m_bRefresh = false;
  m_pAdapter->m_Entries.Clear();

  plHybridArray<plUuid, 16> subAssets;
  {
    plAssetCurator::plLockedSubAsset pAsset = plAssetCurator::GetSingleton()->GetSubAsset(m_pDocument->GetGuid());
    if (pAsset != nullptr && pAsset->m_pAssetInfo != nullptr)
    {
      for (const plUuid& guid : pAsset->m_pAssetInfo->m_SubAssets)
        subAssets.PushBack(guid);
    }
  }

  for (const plUuid& guid : subAssets)
  {
    plAssetCurator::plLockedSubAsset pSubAsset = plAssetCurator::GetSingleton()->GetSubAsset(guid);
    if (pSubAsset == nullptr)
      continue;

    Adapter::Entry& entry = m_pAdapter->m_Entries.ExpandAndGetRef();
    entry.m_sName = pSubAsset->GetName();

    plStringBuilder sGuid;
    plConversionUtils::ToString(guid, sGuid);
    entry.m_sGuid = sGuid;
  }

  m_pAdapter->m_Entries.Sort([](const Adapter::Entry& a, const Adapter::Entry& b)
    { return a.m_sName < b.m_sName; });

  plStringBuilder sCount;
  sCount.SetFormat("{}", m_pAdapter->m_Entries.GetCount());
  GetSurface()->GetBlackboard().SetEntryValue(s_sCount, plString(sCount));

  m_pTree->Refresh();
}

void plFmodSubAssetsView::OnUiUpdate()
{
  if (!IsReady())
    return;

  if (m_bRebuild)
    BuildDocument();

  if (m_bRefresh)
    Refresh();
}

void plFmodSubAssetsView::BuildDocument()
{
  using namespace plEditorUi;

  m_bRebuild = false;

  plUiPanel* pRoot = Panel();
  pRoot->m_Color = Surface();

  SectionHeader(pRoot, "Sub-assets", plFmodEngineUi::AudioColor(), s_sCount.GetView());

  plUiPanel* pList = Panel(pRoot);
  Fill(pList, plVec4(0, SectionHeaderHeight + 2.0f, 0, 0));
  pList->m_Color = Surface();
  m_pTree->Build(pList);

  SetDocument(Finish(pRoot));
  m_bRefresh = true;
}

//////////////////////////////////////////////////////////////////////////
// plFmodEventEmptyView

plFmodEventEmptyView::plFmodEventEmptyView(plDocument* pDocument)
  : plEditorUiContent("FmodEventEmpty")
  , m_pDocument(pDocument)
{
}

void plFmodEventEmptyView::OnUiUpdate()
{
  using namespace plEditorUi;

  if (!IsReady())
    return;

  if (!m_bBuilt)
  {
    m_bBuilt = true;

    plUiPanel* pRoot = Panel();
    pRoot->m_Color = Surface();
    DrawList(pRoot, "Fmod.Empty", s_sEmptyDrawing.GetView());
    SetDocument(Finish(pRoot));
  }

  const plVec2 size = GetSurface()->GetLogicalSize();
  if (size == m_vDrawnSize)
    return;

  m_vDrawnSize = size;
  m_Writer.Clear();

  // the bank the event belongs to, when it is a bank's sub-asset
  plStringBuilder sLine = "Preview is not implemented in the package.";
  {
    plAssetCurator::plLockedSubAsset pAsset = plAssetCurator::GetSingleton()->GetSubAsset(m_pDocument->GetGuid());
    if (pAsset != nullptr && !pAsset->m_bMainAsset && pAsset->m_pAssetInfo != nullptr)
      sLine.SetFormat("Sub-asset of {}. Preview is not implemented in the package.", plStringView(pAsset->m_pAssetInfo->m_Path.GetDataDirParentRelativePath()).GetFileNameAndExtension());
  }

  // an empty state at the top of the panel: icon, line, hint
  const float y = 26.0f;
  plString sIcon;
  SvgTexture(MonochromeIconPath(plFmodEngineUi::IconEvent), &sIcon);
  if (!sIcon.IsEmpty())
    m_Writer.Image(plRectFloat(size.x * 0.5f - 11.0f, y, 22.0f, 22.0f), sIcon, Dim());

  m_Writer.Text(plRectFloat(16.0f, y + 30.0f, size.x - 32.0f, 18.0f), "This sound event has no properties", Muted(), FontSize, plUiTextHAlign::Center);
  m_Writer.Text(plRectFloat(16.0f, y + 52.0f, size.x - 32.0f, 18.0f), sLine, Dim(), FontSize - 2, plUiTextHAlign::Center);

  m_Writer.Publish(GetSurface()->GetBlackboard(), s_sEmptyDrawing);
}
