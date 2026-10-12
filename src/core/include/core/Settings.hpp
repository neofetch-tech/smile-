#pragma once

#include <string>
#include <unordered_map>
#include <QKeySequence>

namespace smile::core {

struct InputConfig {
    QKeySequence up{Qt::Key_W};
    QKeySequence down{Qt::Key_S};
    QKeySequence left{Qt::Key_A};
    QKeySequence right{Qt::Key_D};
    QKeySequence a{Qt::Key_J};
    QKeySequence b{Qt::Key_K};
    QKeySequence start{Qt::Key_Return};
    QKeySequence select{Qt::Key_Shift};
};

struct EmulatorSettings {
    int windowWidth{800};
    int windowHeight{600};
    bool maintainAspectRatio{true};
    bool vSync{true};
    InputConfig inputConfig;
};

} // namespace smile::core
