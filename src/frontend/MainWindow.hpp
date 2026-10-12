#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <memory>
#include <vector>
#include "EmulatorViewport.hpp"
#include "AudioPlayer.hpp"
#include "core/ISystem.hpp"
#include "gameboy/GameBoySystem.hpp"
#include "nes/NesSystem.hpp"

// Launcher flow (DuckStation-simple):
// pick a ROM, double-click a recent entry, or drop a file onto the window,
// then press START. Save states live in the toolbar (F5 = save, F8 = load).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onSelectRom();
    void onStart();
    void onRecentActivated(QListWidgetItem* item);
    void onBackToLauncher();
    void onPauseResume();
    void onReset();
    void onSaveState();
    void onLoadState();
    void onEmulatorTick();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildLauncherPage();
    void buildEmulatorPage();
    void applyMonochromeTheme();
    static uint32_t keyToButton(int key);
    void pushControllerState();
    void applyScale(int scale);

    // Loads ROM bytes from disk and updates the launcher UI (no boot).
    bool loadRomFile(const QString& path);
    // Boots m_romData on the selected core and switches to the game page.
    void startEmulation();

    void refreshRecentList();
    void addRecent(const QString& path);
    [[nodiscard]] QString stateFilePath() const;
    [[nodiscard]] QString batteryPath() const;
    void flushBattery();
    void toggleFullscreen();

    QStackedWidget* m_pages{nullptr};

    // Launcher widgets
    QLabel* m_logoLabel{nullptr};
    QLineEdit* m_romPathEdit{nullptr};
    QComboBox* m_systemCombo{nullptr};
    QComboBox* m_scaleCombo{nullptr};
    QPushButton* m_startButton{nullptr};
    QListWidget* m_recentList{nullptr};

    // Emulator widgets
    EmulatorViewport* m_viewport{nullptr};
    QPushButton* m_pauseButton{nullptr};

    std::unique_ptr<smile::core::ISystem> m_currentSystem;
    std::vector<uint8_t> m_romData;
    QString m_romPath;
    QString m_romBaseName;
    QTimer m_emulatorTimer;
    QTimer m_saveTimer; // Battery autosave: flushes cartridge RAM to the library
    AudioPlayer m_audio;
    std::vector<float> m_audioScratch;
    QElapsedTimer m_frameTimer;
    QElapsedTimer m_tickClock;
    int m_frameCount{0};
    int m_lastSteps{1};
    uint64_t m_frameUs{0};
    uint32_t m_buttonMask{0};
    bool m_paused{false};
};
