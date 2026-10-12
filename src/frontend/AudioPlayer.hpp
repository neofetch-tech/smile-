#pragma once

#include <QObject>
#include <span>

// Push-mode audio sink: the emulator feeds interleaved stereo float frames,
// Qt plays them on the default output device. Black-and-white UI stays silent
// about it; this class only moves samples.
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer() override;

    void start();
    void stop();
    void suspend(bool paused);

    // Frames are interleaved stereo float32 (-1..1). Truncated to fit the
    // device buffer instead of blocking the emulation thread.
    void write(std::span<const float> frames);

private:
    class Impl;
    Impl* m_impl{nullptr};
};
