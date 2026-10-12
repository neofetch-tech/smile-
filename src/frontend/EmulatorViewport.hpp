#pragma once

#include <QMainWindow>
#include <QOpenGLWidget>
#include <QImage>
#include <memory>
#include "core/ISystem.hpp"

class EmulatorViewport : public QOpenGLWidget {
    Q_OBJECT
public:
    explicit EmulatorViewport(QWidget* parent = nullptr);
    ~EmulatorViewport() override = default;

    void updateFrame(std::span<const uint32_t> pixels, smile::core::Resolution res);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QImage m_frameImage;
};
