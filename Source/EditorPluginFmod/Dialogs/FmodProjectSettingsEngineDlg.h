#pragma once

#include <EditorPluginFmod/Dialogs/FmodProjectSettings.h>
#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <EditorUi/ListEditor.h>
#include <GuiFoundation/EngineUi/EngineDialog.h>

/// \brief FMOD Project Settings drawn by the engine: the platforms in a list with Add ("Add Platform" prompt) and
/// Remove (asks first), and for the selected platform the master bank with its file picker, the speaker mode, the
/// virtual channels (8 to 512) and the sampler rate ("Default" or 8000 to 192000 Hz). A channel count that is not a
/// whole number from 8 to 512, a sampler rate that is not "Default" or a whole number, and the strings bank as the
/// master bank show in the validation line; while one shows, OK and Add are disabled and the selection stays on the
/// platform. OK asks before saving RuntimeConfigs/FmodConfig.ddl when something changed.
class plFmodProjectSettingsEngineDlg : public plEngineDialog
{
public:
  plFmodProjectSettingsEngineDlg();
  ~plFmodProjectSettingsEngineDlg();

  /// Opens the engine dialog; without a UI renderer it logs an error.
  static void Open();

protected:
  void BuildBody(plUiElement* pBody, const plVec2& vSize) override;
  void OnButton(plStringView sId) override;
  void OnUiEvent(const plMsgUiEvent& event) override;
  void OnUiUpdate() override;
  plVec2 GetPreferredSize() const override;

private:
  /// The fields the validation line refuses, as bits.
  enum InvalidField : plUInt8
  {
    InvalidMasterBank = PL_BIT(0),
    InvalidChannels = PL_BIT(1),
    InvalidSamplerRate = PL_BIT(2),
  };

  void BuildPlatformList(plUiElement* pBody, const plRectFloat& rect);
  void BuildPlatformForm(plUiElement* pBody, const plRectFloat& rect);

  plFmodConfiguration* GetCurrentConfig();
  const plFmodConfiguration* GetCurrentConfig() const;
  plString ReadField(const char* szEntry) const;
  void WriteField(const char* szEntry, plStringView sText);

  void FillList();
  void SelectCurrentRow();
  void SelectPlatform(plStringView sPlatform);
  void WriteFields();
  void GrabFields();

  /// The InvalidField bits of the selected platform's fields; 0 when they are valid.
  plUInt8 FindInvalidFields() const;

  /// The message for the selected platform's fields; empty when they are valid.
  plStringView Validate() const;
  void UpdateValidation();
  void CorrectSamplerRateField();

  void AddPlatform();
  void RemovePlatform(plStringView sPlatform);
  void BrowseMasterBank();
  void ShowSpeakerModeMenu();
  void OnListEvent(const plEditorUiListEditorEvent& e);

  plFmodProjectSettings m_Settings;
  plString m_sCurrentPlatform;
  plDynamicArray<plString> m_Platforms;
  plUniquePtr<plEditorUiListEditor> m_pList;
  plEventSubscriptionID m_ListSubscription = 0;
  plRectFloat m_ModeRect = plRectFloat(0, 0, 0, 0);
  plUInt8 m_uiInvalidFields = 0; ///< as last built: the outlined fields, Add disabled when not 0
};
