#include "MainWindow.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QPixmap>
#include <QIcon>
#include <fstream>
#include <span>
#include <algorithm>

namespace {
// QSettings location for launcher preferences (last folder, recents, scale).
constexpr int kMaxRecent = 8;

bool hasRomExtension(const QString& path) {
    const QString lower = path.toLower();
    return lower.endsWith(".gb") || lower.endsWith(".gbc") ||
           lower.endsWith(".bin") || lower.endsWith(".nes");
}
} // namespace

// Joypad bitmask layout shared with GameBoySystem::setControllerState:
// bit0 A, bit1 B, bit2 Select, bit3 Start,
// bit4 Right, bit5 Left, bit6 Up, bit7 Down (1 = pressed).
uint32_t MainWindow::keyToButton(int key) {
    switch (key) {
        case Qt::Key_Right:
        case Qt::Key_D:     return 1u << 4;
        case Qt::Key_Left:
        case Qt::Key_A:     return 1u << 5;
        case Qt::Key_Up:
        case Qt::Key_W:     return 1u << 6;
        case Qt::Key_Down:
        case Qt::Key_S:     return 1u << 7;
        case Qt::Key_X:
        case Qt::Key_J:     return 1u << 0; // A
        case Qt::Key_Z:
        case Qt::Key_K:     return 1u << 1; // B
        case Qt::Key_Shift: return 1u << 2; // Select
        case Qt::Key_Return:
        case Qt::Key_Enter: return 1u << 3; // Start
        default:            return 0;
    }
}

void MainWindow::pushControllerState() {
    if (m_currentSystem) {
        m_currentSystem->setControllerState(0, m_buttonMask);
    }
}

void MainWindow::toggleFullscreen() {
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (!event->isAutoRepeat()) {
        if (event->key() == Qt::Key_F11) {
            toggleFullscreen();
            return;
        }
        if (event->key() == Qt::Key_Escape && isFullScreen()) {
            showNormal();
            return;
        }
        if (const uint32_t btn = keyToButton(event->key()); btn != 0) {
            m_buttonMask |= btn;
            pushControllerState();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event) {
    if (!event->isAutoRepeat()) {
        if (const uint32_t btn = keyToButton(event->key()); btn != 0) {
            m_buttonMask &= ~btn;
            pushControllerState();
            return;
        }
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (!event->mimeData()) {
        return;
    }
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && hasRomExtension(url.toLocalFile())) {
            // Dropped ROM boots immediately: the simplest possible flow.
            if (loadRomFile(url.toLocalFile())) {
                startEmulation();
            }
            return;
        }
    }
}

// Black-on-white theme in the spirit of Rufus: flat, high contrast,
// 1 px black borders, inverted controls on hover/selection. No colors at all.
void MainWindow::applyMonochromeTheme() {
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #FFFFFF;
            color: #000000;
            font-size: 13px;
        }
        QLabel#Title {
            font-size: 34px;
            font-weight: 900;
        }
        QLabel#Subtitle {
            font-size: 12px;
            color: #000000;
        }
        QLabel#Hint, QLabel#SectionCaption {
            font-size: 11px;
            color: #000000;
        }
        QLabel#SectionCaption {
            font-weight: 700;
        }
        QLineEdit {
            background-color: #FFFFFF;
            border: 1px solid #000000;
            padding: 6px 8px;
            selection-background-color: #000000;
            selection-color: #FFFFFF;
        }
        QComboBox {
            background-color: #FFFFFF;
            border: 1px solid #000000;
            padding: 5px 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #FFFFFF;
            border: 1px solid #000000;
            selection-background-color: #000000;
            selection-color: #FFFFFF;
        }
        QListWidget {
            background-color: #FFFFFF;
            border: 1px solid #000000;
            selection-background-color: #000000;
            selection-color: #FFFFFF;
            padding: 4px;
        }
        QPushButton {
            background-color: #FFFFFF;
            color: #000000;
            border: 1px solid #000000;
            padding: 8px 18px;
        }
        QPushButton:hover {
            background-color: #000000;
            color: #FFFFFF;
        }
        QPushButton:disabled {
            background-color: #FFFFFF;
            color: #888888;
            border: 1px solid #888888;
        }
        QPushButton#StartButton {
            font-size: 16px;
            font-weight: 700;
            padding: 12px 18px;
            border-width: 2px;
        }
        QToolBar {
            background-color: #FFFFFF;
            border: none;
            border-bottom: 1px solid #000000;
            spacing: 6px;
        }
        QStatusBar {
            background-color: #FFFFFF;
            border-top: 1px solid #000000;
        }
    )");
}

void MainWindow::buildLauncherPage() {
    auto* page = new QWidget(this);
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(24, 20, 24, 20);

    auto* column = new QVBoxLayout();
    column->setSpacing(8);
    outer->addStretch(1);
    outer->addLayout(column);
    outer->addStretch(2);

    // Logo + title. The pixmap loads only when assets/logo.png was compiled in.
    m_logoLabel = new QLabel(page);
    m_logoLabel->setAlignment(Qt::AlignCenter);
#ifdef SMILE_HAS_LOGO
    if (const QPixmap logo(":/icons/logo.png"); !logo.isNull()) {
        m_logoLabel->setPixmap(logo.scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
#endif
    column->addWidget(m_logoLabel);

    auto* title = new QLabel("smile", page);
    title->setObjectName("Title");
    title->setAlignment(Qt::AlignCenter);
    column->addWidget(title);

    auto* subtitle = new QLabel("multi-system retro emulator", page);
    subtitle->setObjectName("Subtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    column->addWidget(subtitle);
    column->addSpacing(8);

    // ROM picker row: path field + SELECT button.
    auto* romRow = new QHBoxLayout();
    romRow->setSpacing(8);
    m_romPathEdit = new QLineEdit(page);
    m_romPathEdit->setReadOnly(true);
    m_romPathEdit->setPlaceholderText("No ROM selected... (or drop a .gb file here)");
    romRow->addWidget(m_romPathEdit, 1);
    auto* selectButton = new QPushButton("SELECT...", page);
    connect(selectButton, &QPushButton::clicked, this, &MainWindow::onSelectRom);
    romRow->addWidget(selectButton);
    column->addLayout(romRow);

    // System + scale row.
    auto* optsRow = new QHBoxLayout();
    optsRow->setSpacing(8);
    optsRow->addWidget(new QLabel("System:", page));
    m_systemCombo = new QComboBox(page);
    m_systemCombo->addItem("Auto-detect");
    m_systemCombo->addItem("Game Boy (DMG)");
    m_systemCombo->addItem("NES / Dendy");
    optsRow->addWidget(m_systemCombo, 1);
    optsRow->addWidget(new QLabel("Scale:", page));
    m_scaleCombo = new QComboBox(page);
    m_scaleCombo->addItems({"1x", "2x", "3x", "4x"});
    optsRow->addWidget(m_scaleCombo);
    column->addLayout(optsRow);
    column->addSpacing(4);

    // Big START button, enabled only when a ROM is chosen.
    m_startButton = new QPushButton("START", page);
    m_startButton->setObjectName("StartButton");
    m_startButton->setEnabled(false);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    column->addWidget(m_startButton);

    // Recent ROMs: one double-click boots the game.
    auto* recentCaption = new QLabel("RECENT:", page);
    recentCaption->setObjectName("SectionCaption");
    column->addWidget(recentCaption);
    m_recentList = new QListWidget(page);
    m_recentList->setMaximumHeight(110);
    connect(m_recentList, &QListWidget::itemActivated,
            this, &MainWindow::onRecentActivated);
    column->addWidget(m_recentList);

    auto* hint = new QLabel("Arrows / WASD - move    Z - B   X - A    Enter - Start    Shift - Select    F11 - fullscreen", page);
    hint->setObjectName("Hint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    column->addWidget(hint);

    m_pages->addWidget(page);
}

void MainWindow::buildEmulatorPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* toolbar = new QToolBar(page);
    toolbar->addAction("< BACK", this, &MainWindow::onBackToLauncher);
    m_pauseButton = new QPushButton("PAUSE", page);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseResume);
    toolbar->addWidget(m_pauseButton);
    auto* saveButton = new QPushButton("SAVE", page);
    saveButton->setShortcut(QKeySequence("F5"));
    saveButton->setToolTip("Save state (F5)");
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::onSaveState);
    toolbar->addWidget(saveButton);
    auto* loadButton = new QPushButton("LOAD", page);
    loadButton->setShortcut(QKeySequence("F8"));
    loadButton->setToolTip("Load state (F8)");
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadState);
    toolbar->addWidget(loadButton);
    auto* resetButton = new QPushButton("RESET", page);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::onReset);
    toolbar->addWidget(resetButton);
    layout->addWidget(toolbar);

    // Centered viewport on a white background with a black frame feel.
    auto* center = new QHBoxLayout();
    center->addStretch(1);
    m_viewport = new EmulatorViewport(page);
    m_viewport->setStyleSheet("border: 2px solid #000000;");
    center->addWidget(m_viewport);
    center->addStretch(1);
    auto* vCenter = new QVBoxLayout();
    vCenter->addStretch(1);
    vCenter->addLayout(center);
    vCenter->addStretch(1);
    layout->addLayout(vCenter, 1);

    m_pages->addWidget(page);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    QCoreApplication::setOrganizationName("smile");
    QCoreApplication::setApplicationName("smile");

    applyMonochromeTheme();
    setWindowTitle("smile - Multi-System Emulator");
#ifdef SMILE_HAS_LOGO
    setWindowIcon(QIcon(":/icons/logo.png"));
#endif
    setAcceptDrops(true);

    m_pages = new QStackedWidget(this);
    setCentralWidget(m_pages);
    buildLauncherPage();
    buildEmulatorPage();
    m_pages->setCurrentIndex(0);

    // Restore launcher preferences.
    QSettings settings;
    m_scaleCombo->setCurrentIndex(settings.value("scale", 2).toInt());
    m_systemCombo->setCurrentIndex(settings.value("system", 0).toInt());
    refreshRecentList();

    statusBar()->showMessage("Select a ROM to begin.");
    resize(640, 620);
    m_audioScratch.resize(4096); // ~46 ms of stereo audio per tick, drained at 60 Hz

    connect(&m_emulatorTimer, &QTimer::timeout, this, &MainWindow::onEmulatorTick);
    // PreciseTimer is mandatory on Windows: the default coarse timer snaps to
    // the 15.6 ms OS tick, so a 16 ms request fires every ~31 ms and the game
    // would run at half speed before emulation even starts.
    m_emulatorTimer.setTimerType(Qt::PreciseTimer);
    m_emulatorTimer.start(16);

    connect(&m_saveTimer, &QTimer::timeout, this, &MainWindow::flushBattery);
    m_saveTimer.start(5000);
}

void MainWindow::onEmulatorTick() {
    if (!m_currentSystem || m_paused || m_pages->currentIndex() != 1) {
        m_tickClock.invalidate();
        return;
    }
    // Catch-up scheduling: convert real elapsed time into emulated frames so
    // the game holds 59.73 fps even when single ticks arrive late. Clamped to
    // avoid the spiral of death; a huge gap (hidden tab, breakpoint) resyncs.
    qint64 elapsedMs = m_tickClock.isValid() ? m_tickClock.restart() : 17;
    if (elapsedMs < 0) {
        elapsedMs = 0;
    }
    int steps = static_cast<int>((elapsedMs * 59.73) / 1000.0 + 0.5);
    steps = std::clamp(steps, 1, 3);
    if (elapsedMs > 100) {
        steps = 1;
    }
    m_lastSteps = steps;

    // Diagnostic: time the emulation itself (excludes paint + audio).
    m_frameTimer.start();
    for (int i = 0; i < steps; ++i) {
        m_currentSystem->stepFrame();
    }
    m_frameUs += static_cast<uint64_t>(m_frameTimer.nsecsElapsed()) / 1000;
    if (++m_frameCount >= 60) {
        statusBar()->showMessage(
            QString("Emu frame %1 ms (budget 16.6 ms), x%2. F5 save, F8 load.")
                .arg(m_frameUs / 60000.0, 0, 'f', 2)
                .arg(m_lastSteps));
        m_frameCount = 0;
        m_frameUs = 0;
    }
    m_viewport->updateFrame(m_currentSystem->getFrameBuffer(),
                            m_currentSystem->getResolution());
    // Drain everything the APU produced (catch-up ticks make extra frames).
    while (true) {
        const size_t frames = m_currentSystem->readAudioSamples(m_audioScratch);
        if (frames == 0) {
            break;
        }
        m_audio.write(std::span<const float>(m_audioScratch.data(), frames));
    }
}

bool MainWindow::loadRomFile(const QString& path) {
    std::ifstream file(path.toStdString(), std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open ROM file."));
        return false;
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0 || size > 8 * 1024 * 1024) {
        QMessageBox::critical(this, tr("Error"), tr("Suspicious ROM size."));
        return false;
    }

    m_romData.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(m_romData.data()), size)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not read ROM file."));
        m_romData.clear();
        return false;
    }

    const QFileInfo info(path);
    m_romPath = path;
    m_romBaseName = info.completeBaseName();
    m_romPathEdit->setText(path);
    m_startButton->setEnabled(true);

    // Auto-detect the system from the file extension.
    if (m_systemCombo->currentIndex() == 0 && hasRomExtension(path)) {
        m_systemCombo->setCurrentIndex(path.endsWith(".nes", Qt::CaseInsensitive) ? 2 : 1);
    }

    addRecent(path);
    QSettings settings;
    settings.setValue("lastDir", info.absolutePath());
    statusBar()->showMessage(tr("ROM ready. Press START."));
    return true;
}

void MainWindow::startEmulation() {
    if (m_romData.empty()) {
        return;
    }

    // Core selection: explicit combo choice wins, Auto-detect follows the file.
    const bool wantNes =
        m_systemCombo->currentIndex() == 2 ||
        (m_systemCombo->currentIndex() == 0 &&
         m_romPath.endsWith(".nes", Qt::CaseInsensitive));

    std::unique_ptr<smile::core::ISystem> system;
    if (wantNes) {
        auto nesSystem = std::make_unique<smile::systems::nes::NesSystem>();
        if (const auto res = nesSystem->loadRom(m_romData); !res) {
            const bool unsupported =
                res.error() == smile::core::EmulationError::UnsupportedMapper;
            QMessageBox::critical(this, tr("Error"),
                unsupported ? tr("Only NROM/UNROM (mappers 0/2) boot for now. MMC1/MMC3 land next.")
                            : tr("This ROM is not a valid NES image."));
            return;
        }
        system = std::move(nesSystem);
    } else {
        auto gbSystem = std::make_unique<smile::systems::gameboy::GameBoySystem>();
        if (const auto res = gbSystem->loadRom(m_romData); !res) {
            const bool unsupported =
                res.error() == smile::core::EmulationError::UnsupportedMapper;
            QMessageBox::critical(this, tr("Error"),
                unsupported ? tr("Unsupported cartridge mapper (MBC3 and friends land later).")
                            : tr("This ROM is not a valid Game Boy image."));
            return;
        }
        system = std::move(gbSystem);
    }

    QSettings settings;
    settings.setValue("scale", m_scaleCombo->currentIndex());
    settings.setValue("system", m_systemCombo->currentIndex());

    m_currentSystem = std::move(system);
    m_buttonMask = 0;
    m_paused = false;
    m_pauseButton->setText("PAUSE");
    applyScale(m_scaleCombo->currentIndex() + 1);
    m_pages->setCurrentIndex(1);
    m_audio.start();
    m_tickClock.start();
    // Restore the battery save from the library, if this cartridge keeps one.
    QFile batteryFile(batteryPath());
    if (batteryFile.open(QIODevice::ReadOnly)) {
        const QByteArray raw = batteryFile.readAll();
        const std::span<const uint8_t> view(
            reinterpret_cast<const uint8_t*>(raw.constData()),
            static_cast<size_t>(raw.size()));
        m_currentSystem->importBattery(view); // Foreign/wrong-size file: ignored
    }
    statusBar()->showMessage("Running. Arrows/WASD move, X = A, Z = B. F5 save, F8 load.");
}

void MainWindow::onSelectRom() {
    QSettings settings;
    const QString startDir = settings.value("lastDir", QDir::homePath()).toString();
    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("Select ROM"), startDir,
        tr("ROMs (*.gb *.gbc *.nes);;Game Boy (*.gb *.gbc);;NES (*.nes);;All Files (*.*)"));
    if (!fileName.isEmpty()) {
        loadRomFile(fileName);
    }
}

void MainWindow::onStart() {
    startEmulation();
}

void MainWindow::onRecentActivated(QListWidgetItem* item) {
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (loadRomFile(path)) {
        startEmulation();
    }
}

void MainWindow::refreshRecentList() {
    m_recentList->clear();
    const QStringList recent = QSettings().value("recent").toStringList();
    for (const QString& path : recent) {
        if (!QFile::exists(path)) {
            continue; // Prune entries whose files are gone
        }
        const QFileInfo info(path);
        auto* item = new QListWidgetItem(info.fileName(), m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
}

void MainWindow::addRecent(const QString& path) {
    QSettings settings;
    QStringList recent = settings.value("recent").toStringList();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > kMaxRecent) {
        recent.removeLast();
    }
    settings.setValue("recent", recent);
    refreshRecentList();
}

void MainWindow::applyScale(int scale) {
    if (!m_viewport) {
        return;
    }
    // Integer scale of the running system's native resolution.
    smile::core::Resolution res{160, 144};
    if (m_currentSystem) {
        res = m_currentSystem->getResolution();
    }
    m_viewport->setFixedSize(static_cast<int>(res.width) * scale,
                             static_cast<int>(res.height) * scale);
}

QString MainWindow::batteryPath() const {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/saves";
    QDir().mkpath(dir);
    return dir + "/" + m_romBaseName + ".sav";
}

void MainWindow::flushBattery() {
    if (!m_currentSystem || m_romBaseName.isEmpty()) {
        return;
    }
    if (!m_currentSystem->takeBatteryDirty()) {
        return; // Nothing changed: no disk traffic at all
    }
    const std::vector<uint8_t> ram = m_currentSystem->exportBattery();
    if (ram.empty()) {
        return;
    }
    QFile file(batteryPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reinterpret_cast<const char*>(ram.data()),
                   static_cast<qint64>(ram.size()));
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    flushBattery(); // Never lose a save on window close
    QMainWindow::closeEvent(event);
}

QString MainWindow::stateFilePath() const {
    const QString dir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/states";
    QDir().mkpath(dir);
    return dir + "/" + m_romBaseName + ".state";
}

void MainWindow::onSaveState() {
    if (!m_currentSystem || m_romBaseName.isEmpty()) {
        return;
    }
    const std::vector<uint8_t> blob = m_currentSystem->serializeState();
    QFile file(stateFilePath());
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char*>(blob.data()),
                   static_cast<qint64>(blob.size())) != static_cast<qint64>(blob.size())) {
        QMessageBox::critical(this, tr("Error"), tr("Could not write save state."));
        return;
    }
    statusBar()->showMessage("State saved (F5).");
}

void MainWindow::onLoadState() {
    if (!m_currentSystem || m_romBaseName.isEmpty()) {
        return;
    }
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, tr("No save"), tr("No save state exists for this ROM yet."));
        return;
    }
    const QByteArray raw = file.readAll();
    const std::span<const uint8_t> view(
        reinterpret_cast<const uint8_t*>(raw.constData()),
        static_cast<size_t>(raw.size()));
    if (!m_currentSystem->deserializeState(view)) {
        QMessageBox::critical(this, tr("Error"), tr("Save state is corrupt or from another ROM."));
        return;
    }
    m_viewport->updateFrame(m_currentSystem->getFrameBuffer(),
                            m_currentSystem->getResolution());
    statusBar()->showMessage("State loaded (F8).");
}

void MainWindow::onBackToLauncher() {
    if (isFullScreen()) {
        showNormal();
    }
    flushBattery();
    m_audio.stop();
    m_pages->setCurrentIndex(0);
    statusBar()->showMessage("Select a ROM to begin.");
}

void MainWindow::onPauseResume() {
    m_paused = !m_paused;
    m_pauseButton->setText(m_paused ? "RESUME" : "PAUSE");
    m_audio.suspend(m_paused);
    statusBar()->showMessage(m_paused ? "Paused." : "Running.");
}

void MainWindow::onReset() {
    if (m_currentSystem) {
        m_currentSystem->reset();
        m_buttonMask = 0;
        m_paused = false;
        m_pauseButton->setText("PAUSE");
        m_audio.suspend(false);
        statusBar()->showMessage("Running.");
    }
}
