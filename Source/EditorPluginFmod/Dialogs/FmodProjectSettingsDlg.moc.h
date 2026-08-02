#pragma once

#include <EditorPluginFmod/EditorPluginFmodDLL.h>
#include <EditorPluginFmod/ui_FmodProjectSettingsDlg.h>
#include <FmodPlugin/FmodSingleton.h>
#include <QDialog>

class plQtFmodProjectSettingsDlg : public QDialog, public Ui_FmodProjectSettingsDlg
{
public:
  Q_OBJECT

public:
  plQtFmodProjectSettingsDlg(QWidget* pParent);

private Q_SLOTS:
  void on_ButtonBox_clicked(QAbstractButton* pButton);
  void on_ListPlatforms_itemSelectionChanged();
  void on_ButtonAdd_clicked();
  void on_ButtonRemove_clicked();
  void on_ButtonMB_clicked();

private:
  plResult Save();
  void Load();
  void SetCurrentPlatform(const char* szPlatform);
  void StoreCurrentPlatform();

  plString m_sCurrentPlatform;
  plFmodAssetProfiles m_ConfigsOld;
  plFmodAssetProfiles m_Configs;
};
