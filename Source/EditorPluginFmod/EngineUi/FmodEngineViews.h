#pragma once

#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <EditorUi/Content.h>
#include <EditorUi/Tree.h>
#include <Foundation/Types/UniquePtr.h>
#include <GameUiPlugin/Elements/UiDrawList.h>

#include <memory>

class plDocument;
struct plAssetCuratorEvent;

namespace plFmodEngineUi
{
  /// The Audio category colour.
  plColorGammaUB AudioColor();

  /// Package artwork for the engine views (":/AssetIcons/" resolves to the SDK's own artwork).
  inline constexpr const char* IconEvent = ":/FmodIcons/Sound_Event.svg";
  inline constexpr const char* IconBank = ":/FmodIcons/Sound_Bank.svg";
} // namespace plFmodEngineUi

/// \brief The "Sub-assets" list of the FMOD Sound Bank window: a section header with the count and the event:/ paths
/// the bank provides. Double-click opens one.
///
/// The FMOD views are editor UI contents: the engine shell shows them in the windows' panels (plFmodShellPanels).
class plFmodSubAssetsView : public plEditorUiContent
{
public:
  explicit plFmodSubAssetsView(plDocument* pDocument);
  ~plFmodSubAssetsView() override;

  /// The height the list gets under the bank's properties.
  static constexpr float Height = 220.0f;

protected:
  void OnUiUpdate() override;

private:
  class Adapter;

  void BuildDocument();
  void Refresh();
  void AssetCuratorEventHandler(const plAssetCuratorEvent& e);

  plDocument* m_pDocument = nullptr;
  plUniquePtr<Adapter> m_pAdapter;
  plUniquePtr<plEditorUiTree> m_pTree;
  bool m_bRebuild = true;
  bool m_bRefresh = true;

  /// Refreshes queued from other threads run only while the view lives.
  std::shared_ptr<bool> m_pAlive;
};

/// \brief The body of the FMOD Sound Event window, which has no properties: an empty state with the bank the event
/// belongs to (when it is a bank's sub-asset) and that the package has no preview.
class plFmodEventEmptyView : public plEditorUiContent
{
public:
  explicit plFmodEventEmptyView(plDocument* pDocument);

protected:
  void OnUiUpdate() override;

private:
  plDocument* m_pDocument = nullptr;
  plVec2 m_vDrawnSize = plVec2::MakeZero();
  bool m_bBuilt = false;
  plUiDrawListWriter m_Writer;
};
