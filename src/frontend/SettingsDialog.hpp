#pragma once

#include <QDialog>
#include <QKeySequenceEdit>
#include <QSpinBox>
#include <QCheckBox>
#include "core/Settings.hpp"

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(smile::core::Settings& settings, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

private:
    void saveSettings();

    smile::core::Settings& m_settings;

    QKeySequenceEdit* m_keyUp{nullptr};
    QKeySequenceEdit* m_keyDown{nullptr};
    QKeySequenceEdit* m_keyLeft{nullptr};
    QKeySequenceEdit* m_keyRight{nullptr};
    QKeySequenceEdit* m_keyA{nullptr};
    QKeySequenceEdit* m_keyB{nullptr};
    QKeySequenceEdit* m_keyStart{nullptr};
    QKeySequenceEdit* m_keySelect{nullptr};
    QCheckBox* m_checkVSync{nullptr};
};
