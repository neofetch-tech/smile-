#include "EmulatorViewport.hpp"
#include <QPainter>
#include <cstring>

EmulatorViewport::EmulatorViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void EmulatorViewport::updateFrame(std::span<const uint32_t> pixels, smile::core::Resolution res) {
    if (res.width == 0 || res.height == 0) return;

    if (m_frameImage.width() != static_cast<int>(res.width) || 
        m_frameImage.height() != static_cast<int>(res.height)) {
        m_frameImage = QImage(res.width, res.height, QImage::Format_ARGB32);
        m_frameImage.fill(Qt::black);
    }

    if (!pixels.empty() && pixels.size() >= res.width * res.height) {
        std::memcpy(m_frameImage.bits(), pixels.data(), pixels.size() * sizeof(uint32_t));
    }

    update();
}

void EmulatorViewport::initializeGL() {
    // Basic OpenGL initialization if needed
}

void EmulatorViewport::paintGL() {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    
    if (!m_frameImage.isNull()) {
        painter.drawImage(rect(), m_frameImage);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
}

void EmulatorViewport::resizeGL(int /*w*/, int /*h*/) {
    // Handle viewport resize if needed
}
