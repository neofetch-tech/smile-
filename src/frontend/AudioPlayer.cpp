#include "AudioPlayer.hpp"

#include <QAudioFormat>
#include <QAudioSink>
#include <QIODevice>
#include <QMediaDevices>
#include <algorithm>
#include <vector>

class AudioPlayer::Impl {
public:
    QAudioSink* sink{nullptr};
    QIODevice* device{nullptr};
};

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent), m_impl(new Impl()) {
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Float);
    m_impl->sink = new QAudioSink(QMediaDevices::defaultAudioOutput(), format, this);
    // A fixed device buffer (~185 ms) rides out scheduling jitter without
    // adding noticeable latency for chiptune music and effects.
    m_impl->sink->setBufferSize(32768);
}

AudioPlayer::~AudioPlayer() {
    stop();
    delete m_impl;
}

void AudioPlayer::start() {
    if (m_impl->sink && !m_impl->device) {
        m_impl->device = m_impl->sink->start();
        if (m_impl->device) {
            // Pre-roll ~90 ms of digital silence so the first real samples
            // never play into an empty buffer (startup crackle).
            static const std::vector<char> silence(16384, 0);
            m_impl->device->write(silence.data(), static_cast<qint64>(silence.size()));
        }
    } else if (m_impl->sink) {
        m_impl->sink->resume();
    }
}

void AudioPlayer::stop() {
    if (m_impl->sink) {
        m_impl->sink->stop();
    }
    m_impl->device = nullptr;
}

void AudioPlayer::suspend(bool paused) {
    if (!m_impl->sink) {
        return;
    }
    if (paused) {
        m_impl->sink->suspend();
    } else {
        m_impl->sink->resume();
    }
}

void AudioPlayer::write(std::span<const float> frames) {
    if (!m_impl->device || frames.empty()) {
        return;
    }
    // All-or-nothing: a truncated (partial) write would splice a discontinuity
    // into the waveform, which is heard as a click. Dropping the whole chunk
    // under underrun pressure sounds far cleaner.
    const size_t needed = frames.size() * sizeof(float);
    if (static_cast<size_t>(m_impl->sink->bytesFree()) < needed) {
        return;
    }
    m_impl->device->write(reinterpret_cast<const char*>(frames.data()),
                          static_cast<qint64>(needed));
}
