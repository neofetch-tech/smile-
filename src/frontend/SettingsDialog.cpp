#include "SettingsDialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGroupBox>

SettingsDialog::SettingsDialog(smile::core::Settings& settings, QWidget* parent)
    : QDialog(parent), m_settings(settings) {
    setWindowTitle(tr("Emulator Settings"));
    resize(400, 400);

    auto* mainLayout = new QVBoxLayout(this);

    // Controls group
    auto* controlsGroup = new QGroupBox(tr("Controls (WASD / Arrow Keys)"), this);
    auto* formLayout = new QFormLayout(controlsGroup);

    m_keyUp = new QKeySequenceEdit(m_settings.inputConfig.up, this);
    m_keyDown = new QKeySequenceEdit(m_settings.inputConfig.down, this);
    m_keyLeft = new QKeySequenceEdit(m_settings.inputConfig.left, this);
    m_keyRight = new QKeySequenceEdit(m_settings.inputConfig.right, this);
    m_keyA = new QKeySequenceEdit(m_settings.inputConfig.a, this);
    m_keyB = new QKeySequenceEdit(m_settings.inputConfig.b, this);
    m_keyStart = new QKeySequenceEdit(m_settings.inputConfig.start, this);
    m_keySelect = new QKeySequenceEdit(m_settings.inputConfig.select, this);

    formLayout->addRow(tr("Up:"), m_keyUp);
    formLayout->addRow(tr("Down:"), m_keyDown);
    formLayout->addRow(tr("Left:"), m_keyLeft);
    formLayout->addRow(tr("Right:"), m_keyRight);
    formLayout->addRow(tr("Button A:"), m_keyA);
    formLayout->addRow(tr("Button B:"), m_keyB);
    formLayout->addRow(tr("Start:"), m_keyStart);
    formLayout->addRow(tr("Select:"), m_keySelect);

    mainLayout->addWidget(controlsGroup);

    // Video/Display group
    auto* videoGroup = new QGroupBox(tr("Display & Video"), this);
    auto* videoLayout = new QVBoxLayout(videoGroup);
    m_checkVSync = new QCheckBox(tr("Enable VSync"), this);
    m_checkVSync->setChecked(m_settings.vSync);
    videoLayout->addWidget(m_checkVSync);

    mainLayout->addWidget(videoGroup);

    // Buttons
    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::saveSettings() {
    m_settings.inputConfig.up = m_keyUp->keySequence();
    m_settings.inputConfig.down = m_keyDown->keySequence();
    m_settings.inputConfig.left = m_keyLeft->keySequence();
    m_settings.inputConfig.right = m_keyRight->keySequence();
    m_settings.inputConfig.a = m_keyA->keySequence();
    m_settings.inputConfig.b = m_keyB->keySequence();
    m_settings.inputConfig.start = m_keyStart->keySequence();
    m_settings.inputConfig.select = m_keySelect->keySequence();
    m_settings.vSync = m_checkVSync->isChecked();
}
