#include <EditorPluginFmod/EditorPluginFmodPCH.h>

#include <EditorPluginFmod/Dialogs/FmodProjectSettingsEngineDlg.h>
#include <EditorUi/Menu.h>
#include <EditorUi/Surface.h>
#include <Foundation/Utilities/ConversionUtils.h>
#include <GameUiPlugin/Components/UiCanvasComponent.h>

namespace
{
  const char* s_szOk = "Ok";
  const char* s_szCancel = "Cancel";

  constexpr float s_fWidth = 600.0f;
  constexpr float s_fListWidth = 170.0f;
  constexpr float s_fListHeight = 220.0f;
  constexpr float s_fStripHeight = 30.0f;
  constexpr float s_fStripButton = 22.0f;
  constexpr float s_fColumnGap = 12.0f;
  constexpr float s_fLabelWidth = 120.0f;
  constexpr float s_fNumberWidth = 110.0f;

  constexpr plUInt32 s_uiMinChannels = 8;
  constexpr plUInt32 s_uiMaxChannels = 512;
  constexpr plUInt32 s_uiMinSamplerRate = 8000;
  constexpr plUInt32 s_uiMaxSamplerRate = 192000;

  const char* s_szBankText = "Fmod.MasterBank.Text";
  const char* s_szChannelsText = "Fmod.Channels.Text";
  const char* s_szRateText = "Fmod.SamplerRate.Text";

  const char* const s_szSpeakerModes[] = {"Stereo", "5.1", "7.1"};
  const plFmodSpeakerMode s_SpeakerModes[] = {plFmodSpeakerMode::ModeStereo, plFmodSpeakerMode::Mode5Point1, plFmodSpeakerMode::Mode7Point1};

  const char* s_szBankTip = "The relative path to the master bank. Or the GUID of the master bank asset.";
  const char* s_szModeTip = "Must be identical to what has been configured in FMOD Studio for this platform.";
  const char* s_szChannelsTip = "The maximum number of sounds that are audible simultaneously.";
  const char* s_szRateTip = "Default uses the platform rate";
  const char* s_szRuntimeHint = "The runtime reads \"Desktop\" (Windows, Linux, macOS) and \"Mobile\" (Android, iOS).";

  const char* s_szChannelsMessage = "Virtual Channels is 8 to 512.";
  const char* s_szStringsBankMessage = "The strings sound bank cannot be selected as the master sound bank.";
  const char* s_szSamplerRateMessage = "Sampler Rate is Default or 8000 to 192000 Hz.";

  plUInt32 GetSpeakerModeIndex(plFmodSpeakerMode mode)
  {
    switch (mode)
    {
      case plFmodSpeakerMode::ModeStereo:
        return 0;
      case plFmodSpeakerMode::Mode7Point1:
        return 2;
      default:
        return 1;
    }
  }

  /// A field's text as a whole number without a sign; false for anything else, including trailing characters.
  bool ParseWholeNumber(plStringView sText, plUInt32& out_uiValue)
  {
    plStringBuilder sTrimmed(sText);
    sTrimmed.Trim(" \t");

    if (sTrimmed.IsEmpty() || !plStringUtils::IsDecimalDigit(*sTrimmed.GetData()))
      return false;

    const char* szEnd = nullptr;
    if (plConversionUtils::StringToUInt(sTrimmed, out_uiValue, &szEnd).Failed())
      return false;

    return szEnd == sTrimmed.GetData() + sTrimmed.GetElementCount();
  }

  /// A channel count: a whole number from 8 to 512.
  bool ParseChannels(plStringView sText, plUInt32& out_uiChannels)
  {
    plUInt32 uiValue = 0;
    if (!ParseWholeNumber(sText, uiValue) || uiValue < s_uiMinChannels || uiValue > s_uiMaxChannels)
      return false;

    out_uiChannels = uiValue;
    return true;
  }

  /// A sampler rate: "Default" (or nothing) is the default rate (0); a whole number is corrected: rates under 8000 to the
  /// default rate, rates over 192000 to 192000. False for any other text.
  bool ParseSamplerRate(plStringView sText, plUInt32& out_uiRate)
  {
    plStringBuilder sTrimmed(sText);
    sTrimmed.Trim(" \t");

    if (sTrimmed.IsEmpty() || sTrimmed.IsEqual_NoCase("Default"))
    {
      out_uiRate = 0;
      return true;
    }

    plUInt32 uiValue = 0;
    if (!ParseWholeNumber(sTrimmed, uiValue))
      return false;

    out_uiRate = uiValue < s_uiMinSamplerRate ? 0 : plMath::Min(uiValue, s_uiMaxSamplerRate);
    return true;
  }

  plString FormatSamplerRate(plUInt32 uiRate)
  {
    if (uiRate == 0)
      return "Default";

    plStringBuilder sValue;
    sValue.SetFormat("{}", uiRate);
    return sValue;
  }

  bool IsStringsBank(plStringView sPath)
  {
    return sPath.EndsWith_NoCase(".strings.bank");
  }

  /// A danger outline over a field the validation line refuses.
  void MarkInvalid(plUiElement* pParent, const plRectFloat& rect)
  {
    plUiPanel* pMark = plEditorUi::Panel(pParent);
    plEditorUi::SetRect(pMark, rect);
    pMark->m_Color = plColorGammaUB(0, 0, 0, 0);
    pMark->m_fBorderWidth = 1.0f;
    pMark->m_BorderColor = plEditorUi::Danger();
    pMark->m_vCornerRadius = plVec4(4);
  }
} // namespace

plFmodProjectSettingsEngineDlg::plFmodProjectSettingsEngineDlg()
  : plEngineDialog("FmodProjectSettings", "FMOD Project Settings")
{
  SetIcon(":/TypeIcons/plFmodEventComponent.svg");
  m_Settings.Load();
  m_sCurrentPlatform = m_Settings.GetInitialPlatform();

  if (!IsReady())
    return;

  plEditorUiDialogButton buttons[2];
  buttons[0].m_sId = s_szOk;
  buttons[0].m_sLabel = "OK";
  buttons[0].m_Role = plEditorUiDialogButton::Role::Accept;
  buttons[1].m_sId = s_szCancel;
  buttons[1].m_sLabel = "Cancel";
  buttons[1].m_Role = plEditorUiDialogButton::Role::Reject;
  GetHost().SetButtons(buttons);

  m_pList = PL_DEFAULT_NEW(plEditorUiListEditor, *GetSurface(), "Fmod.Platforms");
  m_pList->SetRenamable(false);
  m_pList->SetReorderable(false);
  m_ListSubscription = m_pList->m_Events.AddEventHandler(plMakeDelegate(&plFmodProjectSettingsEngineDlg::OnListEvent, this));

  FillList();
  WriteFields();
}

plFmodProjectSettingsEngineDlg::~plFmodProjectSettingsEngineDlg()
{
  if (m_pList != nullptr)
    m_pList->m_Events.RemoveEventHandler(m_ListSubscription);
}

void plFmodProjectSettingsEngineDlg::Open()
{
  if (!plEngineDialog::IsEngineEnabled())
  {
    plLog::Error("The FMOD project settings dialog needs the editor UI renderer.");
    return;
  }

  plFmodProjectSettingsEngineDlg dlg;
  dlg.Exec();
}

plVec2 plFmodProjectSettingsEngineDlg::GetPreferredSize() const
{
  const float fPad = plEditorUiDialogHost::Padding;
  return plVec2(s_fWidth, fPad + s_fListHeight + s_fStripHeight + fPad + plEditorUiDialogHost::FooterHeight);
}

plFmodConfiguration* plFmodProjectSettingsEngineDlg::GetCurrentConfig()
{
  auto it = m_Settings.m_Configs.m_AssetProfiles.Find(m_sCurrentPlatform);
  return it.IsValid() ? &it.Value() : nullptr;
}

const plFmodConfiguration* plFmodProjectSettingsEngineDlg::GetCurrentConfig() const
{
  auto it = m_Settings.m_Configs.m_AssetProfiles.Find(m_sCurrentPlatform);
  return it.IsValid() ? &it.Value() : nullptr;
}

plString plFmodProjectSettingsEngineDlg::ReadField(const char* szEntry) const
{
  const plVariant value = GetSurface()->GetBlackboard().GetEntryValue(plTempHashedString(szEntry));
  return value.IsValid() ? value.ConvertTo<plString>() : plString();
}

void plFmodProjectSettingsEngineDlg::WriteField(const char* szEntry, plStringView sText)
{
  GetSurface()->GetCanvas().SetTextBinding(szEntry, sText);
  GetSurface()->GetBlackboard().SetEntryValue(szEntry, plString(sText));
}

void plFmodProjectSettingsEngineDlg::FillList()
{
  m_Platforms.Clear();

  plDynamicArray<plEditorUiListItem> items;
  for (auto it = m_Settings.m_Configs.m_AssetProfiles.GetIterator(); it.IsValid(); ++it)
  {
    m_Platforms.PushBack(it.Key());

    plEditorUiListItem& item = items.ExpandAndGetRef();
    item.m_sText = it.Key();

    // the runtime picks one of these two by the platform it runs on
    if (it.Key() == "Desktop" || it.Key() == "Mobile")
      item.m_sNote = "used";
  }

  m_pList->SetItems(items);
  SelectCurrentRow();
}

void plFmodProjectSettingsEngineDlg::SelectPlatform(plStringView sPlatform)
{
  GrabFields();

  m_sCurrentPlatform = sPlatform;

  WriteFields();
  Rebuild();
}

void plFmodProjectSettingsEngineDlg::WriteFields()
{
  const plFmodConfiguration* pConfig = GetCurrentConfig();
  if (pConfig == nullptr)
    return;

  WriteField(s_szBankText, pConfig->m_sMasterSoundBank);

  plStringBuilder sValue;
  sValue.SetFormat("{}", pConfig->m_uiVirtualChannels);
  WriteField(s_szChannelsText, sValue);

  WriteField(s_szRateText, FormatSamplerRate(pConfig->m_uiSamplerRate));
}

void plFmodProjectSettingsEngineDlg::GrabFields()
{
  plFmodConfiguration* pConfig = GetCurrentConfig();
  if (pConfig == nullptr)
    return;

  // only valid values are taken: the selection, Add and OK wait until the validation line is clear
  const plString sBank = ReadField(s_szBankText);
  if (!IsStringsBank(sBank))
    pConfig->m_sMasterSoundBank = sBank;

  plUInt32 uiChannels = 0;
  if (ParseChannels(ReadField(s_szChannelsText), uiChannels))
    pConfig->m_uiVirtualChannels = static_cast<plUInt16>(uiChannels);

  plUInt32 uiRate = 0;
  if (ParseSamplerRate(ReadField(s_szRateText), uiRate))
    pConfig->m_uiSamplerRate = uiRate;
}

plUInt8 plFmodProjectSettingsEngineDlg::FindInvalidFields() const
{
  if (GetCurrentConfig() == nullptr)
    return 0;

  plUInt8 uiInvalid = 0;
  plUInt32 uiValue = 0;

  if (IsStringsBank(ReadField(s_szBankText)))
    uiInvalid |= InvalidMasterBank;

  if (!ParseChannels(ReadField(s_szChannelsText), uiValue))
    uiInvalid |= InvalidChannels;

  if (!ParseSamplerRate(ReadField(s_szRateText), uiValue))
    uiInvalid |= InvalidSamplerRate;

  return uiInvalid;
}

plStringView plFmodProjectSettingsEngineDlg::Validate() const
{
  const plUInt8 uiInvalid = FindInvalidFields();

  if (uiInvalid & InvalidChannels)
    return s_szChannelsMessage;

  if (uiInvalid & InvalidMasterBank)
    return s_szStringsBankMessage;

  if (uiInvalid & InvalidSamplerRate)
    return s_szSamplerRateMessage;

  return {};
}

void plFmodProjectSettingsEngineDlg::UpdateValidation()
{
  const plStringView sMessage = Validate();

  GetHost().SetValidation(sMessage.IsEmpty() ? plEditorUiSeverity::None : plEditorUiSeverity::Error, sMessage);
  GetHost().SetButtonEnabled(s_szOk, sMessage.IsEmpty());

  // the outlines and the Add button follow the refused fields
  const plUInt8 uiInvalid = FindInvalidFields();
  if (uiInvalid != m_uiInvalidFields)
  {
    m_uiInvalidFields = uiInvalid;
    Rebuild();
  }
}

void plFmodProjectSettingsEngineDlg::CorrectSamplerRateField()
{
  if (GetCurrentConfig() == nullptr || GetSurface()->GetCanvas().GetFocusedElementName() == "Fmod.SamplerRate")
    return;

  // once the field loses focus it shows the rate it stores
  const plString sText = ReadField(s_szRateText);

  plUInt32 uiRate = 0;
  if (ParseSamplerRate(sText, uiRate) && sText != FormatSamplerRate(uiRate))
    WriteField(s_szRateText, FormatSamplerRate(uiRate));
}

void plFmodProjectSettingsEngineDlg::OnUiUpdate()
{
  CorrectSamplerRateField();
  UpdateValidation();
}

void plFmodProjectSettingsEngineDlg::BuildBody(plUiElement* pBody, const plVec2& vSize)
{
  using namespace plEditorUi;

  const float fPad = plEditorUiDialogHost::Padding;
  const float fHeight = plMath::Max(vSize.y - 2.0f * fPad, s_fStripHeight + 40.0f);

  const plRectFloat list(fPad, fPad, s_fListWidth, fHeight);
  BuildPlatformList(pBody, list);

  const float fFormX = list.Right() + s_fColumnGap;
  const plRectFloat form(fFormX, fPad, plMath::Max(vSize.x - fFormX - fPad, 0.0f), fHeight);

  if (GetCurrentConfig() != nullptr)
  {
    BuildPlatformForm(pBody, form);
    return;
  }

  plUiPanel* pEmpty = Panel(pBody);
  SetRect(pEmpty, form);
  pEmpty->m_Color = plColorGammaUB(0, 0, 0, 0);
  EmptyState(pEmpty, Icon::Volume, "No platform selected.", {});
}

void plFmodProjectSettingsEngineDlg::BuildPlatformList(plUiElement* pBody, const plRectFloat& rect)
{
  using namespace plEditorUi;

  plBlackboard& board = GetSurface()->GetBlackboard();
  const float fListHeight = rect.height - s_fStripHeight;

  plUiPanel* pFrame = Panel(pBody);
  SetRect(pFrame, rect);
  pFrame->m_Color = Field();
  pFrame->m_fBorderWidth = 1.0f;
  pFrame->m_BorderColor = Line();
  pFrame->m_vCornerRadius = plVec4(4);

  plUiPanel* pListArea = Panel(pFrame);
  SetRect(pListArea, plRectFloat(1.0f, 1.0f, rect.width - 2.0f, fListHeight - 1.0f));
  pListArea->m_Color = plColorGammaUB(0, 0, 0, 0);
  m_pList->Build(pListArea);

  // the strip under the list: its caption, Add and Remove
  plUiPanel* pRule = Panel(pFrame);
  SetRect(pRule, plRectFloat(0.0f, fListHeight, rect.width, 1.0f));
  pRule->m_Color = Line();

  plUiPanel* pStrip = Panel(pFrame);
  SetRect(pStrip, plRectFloat(1.0f, fListHeight + 1.0f, rect.width - 2.0f, s_fStripHeight - 2.0f));
  pStrip->m_Color = Surface();
  pStrip->m_vCornerRadius = plVec4(0, 0, 3, 3);

  TextAt(pFrame, "Platform", plRectFloat(8.0f, fListHeight, 80.0f, s_fStripHeight), Dim(), FontSize - 2);

  // Add waits while the selected platform has refused values, which a new selection would drop
  const float fButtonY = fListHeight + (s_fStripHeight - s_fStripButton) * 0.5f;
  IconPushButton(pFrame, board, plRectFloat(rect.width - 2.0f * s_fStripButton - 6.0f, fButtonY, s_fStripButton, s_fStripButton), IconPath(Icon::Add), "Fmod.Add", m_uiInvalidFields == 0);
  IconPushButton(pFrame, board, plRectFloat(rect.width - s_fStripButton - 4.0f, fButtonY, s_fStripButton, s_fStripButton), IconPath(Icon::Remove), "Fmod.Remove", GetCurrentConfig() != nullptr);
  SetToolTip(board, "Fmod.Add", "Add Platform");
  SetToolTip(board, "Fmod.Remove", "Remove Platform");
}

void plFmodProjectSettingsEngineDlg::BuildPlatformForm(plUiElement* pBody, const plRectFloat& rect)
{
  using namespace plEditorUi;

  plBlackboard& board = GetSurface()->GetBlackboard();
  const plFmodConfiguration* pConfig = GetCurrentConfig();

  plEditorUiForm form(pBody, rect, s_fLabelWidth);

  // the master bank: a path field and the file picker
  const plRectFloat bank = form.Row("Masterbank", ControlHeight, s_szBankTip);
  const float fBrowseWidth = PushButtonWidth("...");
  TextField(pBody, plRectFloat(bank.x, bank.y, bank.width - fBrowseWidth - 4.0f, bank.height), "Fmod.MasterBank", s_szBankText, {}, {}, true);
  PushButton(pBody, board, plRectFloat(bank.Right() - fBrowseWidth, bank.y, fBrowseWidth, bank.height), "...", "Fmod.Browse");
  SetToolTip(board, "Fmod.MasterBank", s_szBankTip);
  SetToolTip(board, "Fmod.Browse", "Select Master Sound Bank: Sound Banks (*.bank)");

  m_ModeRect = form.Row("Speaker Mode", ControlHeight, s_szModeTip);
  Dropdown(pBody, board, m_ModeRect, s_szSpeakerModes[GetSpeakerModeIndex(pConfig->m_SpeakerMode)], "Fmod.SpeakerMode");
  SetToolTip(board, "Fmod.SpeakerMode", s_szModeTip);

  const plRectFloat channels = form.Row("Virtual Channels", ControlHeight, s_szChannelsTip);
  const plRectFloat channelsField(channels.x, channels.y, s_fNumberWidth, channels.height);
  NumberField(pBody, channelsField, "Fmod.Channels", s_szChannelsText, {});
  SetToolTip(board, "Fmod.Channels", s_szChannelsTip);

  if (m_uiInvalidFields & InvalidChannels)
    MarkInvalid(pBody, channelsField);

  const plRectFloat rate = form.Row("Sampler Rate", ControlHeight, s_szRateTip);
  const plRectFloat rateField(rate.x, rate.y, s_fNumberWidth, rate.height);
  NumberField(pBody, rateField, "Fmod.SamplerRate", s_szRateText, {}, {}, "Hz");
  SetToolTip(board, "Fmod.SamplerRate", s_szRateTip);

  if (m_uiInvalidFields & InvalidSamplerRate)
    MarkInvalid(pBody, rateField);

  form.Gap(4.0f);
  form.Hint(s_szRuntimeHint, true);

  AddBodyToolTips(form.GetToolTips());
}

void plFmodProjectSettingsEngineDlg::AddPlatform()
{
  if (!Validate().IsEmpty())
    return;

  GrabFields();

  const plString sPlatform = m_Settings.AskAndAddPlatform();
  if (sPlatform.IsEmpty())
    return;

  m_sCurrentPlatform = sPlatform;

  FillList();
  WriteFields();
  Rebuild();
}

void plFmodProjectSettingsEngineDlg::RemovePlatform(plStringView sPlatform)
{
  const plString sName = sPlatform;
  if (!m_Settings.m_Configs.m_AssetProfiles.Contains(sName))
    return;

  if (plEditorUiServices::MessageBoxQuestion(plFmt("Remove the platform '{}'?", sName), plEditorUiMessageButton::Yes | plEditorUiMessageButton::No, plEditorUiMessageButton::No) != plEditorUiMessageButton::Yes)
    return;

  m_Settings.m_Configs.m_AssetProfiles.Remove(sName);

  // removing another platform keeps the selection and its fields as they are
  if (sName != m_sCurrentPlatform)
  {
    FillList();
    Rebuild();
    return;
  }

  // the first platform in the list takes the selection
  m_sCurrentPlatform = m_Settings.GetFirstPlatform();

  FillList();
  WriteFields();
  Rebuild();
}

void plFmodProjectSettingsEngineDlg::SelectCurrentRow()
{
  const plUInt32 uiIndex = m_Platforms.IndexOf(m_sCurrentPlatform);
  m_pList->SetSelectedIndex(uiIndex == plInvalidIndex ? -1 : static_cast<plInt32>(uiIndex));
}

void plFmodProjectSettingsEngineDlg::BrowseMasterBank()
{
  plString sPath;
  if (plFmodProjectSettings::PickMasterBank(sPath))
    WriteField(s_szBankText, sPath);
}

void plFmodProjectSettingsEngineDlg::ShowSpeakerModeMenu()
{
  const plFmodConfiguration* pConfig = GetCurrentConfig();
  if (pConfig == nullptr)
    return;

  const plUInt32 uiCurrent = GetSpeakerModeIndex(pConfig->m_SpeakerMode);

  plEditorUiMenu menu;
  for (plUInt32 i = 0; i < PL_ARRAY_SIZE(s_szSpeakerModes); ++i)
  {
    plEditorUiMenuItem& item = menu.AddAction(s_szSpeakerModes[i], i);
    item.m_bCheckable = true;
    item.m_bChecked = i == uiCurrent;
  }

  const plVec2 vPos(GetBodyRect().x + m_ModeRect.x, GetBodyRect().y + m_ModeRect.y + m_ModeRect.height);
  plEditorUiMenuView::Show(*GetSurface(), vPos, menu, [this](plUInt32 uiPicked)
    {
      plFmodConfiguration* pPicked = GetCurrentConfig();
      if (pPicked == nullptr || uiPicked >= PL_ARRAY_SIZE(s_SpeakerModes))
        return;

      pPicked->m_SpeakerMode = s_SpeakerModes[uiPicked];
      Rebuild(); });
}

void plFmodProjectSettingsEngineDlg::OnListEvent(const plEditorUiListEditorEvent& e)
{
  if (e.m_Type == plEditorUiListEditorEvent::Type::SelectionChanged)
  {
    const plInt32 iIndex = m_pList->GetSelectedIndex();
    const plString sPlatform = iIndex >= 0 ? m_Platforms[iIndex] : plString();

    if (sPlatform == m_sCurrentPlatform)
      return;

    // a platform with refused values stays selected until they are fixed, so the typed values are not lost
    if (!Validate().IsEmpty())
    {
      SelectCurrentRow();
      return;
    }

    SelectPlatform(sPlatform);
  }
  else if (e.m_Type == plEditorUiListEditorEvent::Type::RemoveRequested)
  {
    if (e.m_uiIndex < m_Platforms.GetCount())
      RemovePlatform(m_Platforms[e.m_uiIndex]);
  }
}

void plFmodProjectSettingsEngineDlg::OnUiEvent(const plMsgUiEvent& event)
{
  const plStringView sId = event.m_sIdentifier.GetView();

  if (sId == "Fmod.Add")
    AddPlatform();
  else if (sId == "Fmod.Remove")
    RemovePlatform(m_sCurrentPlatform);
  else if (sId == "Fmod.Browse")
    BrowseMasterBank();
  else if (sId == "Fmod.SpeakerMode")
    ShowSpeakerModeMenu();
}

void plFmodProjectSettingsEngineDlg::OnButton(plStringView sId)
{
  if (sId == s_szOk)
  {
    if (!Validate().IsEmpty())
      return;

    GrabFields();

    if (!m_Settings.AskAndSave())
      return;
  }

  plEngineDialog::OnButton(sId);
}
