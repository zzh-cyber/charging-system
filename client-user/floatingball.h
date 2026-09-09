#pragma once

// ============================================================================
// FloatingBall - AI 客服悬浮圆球按钮
// 蓝色圆形按钮，可拖动；点击（非拖动）发出 clicked 信号。
// ============================================================================

#include <QWidget>

class FloatingBall : public QWidget
{
    Q_OBJECT
public:
    explicit FloatingBall(QWidget *parent = nullptr);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QPoint m_pressPos;
    bool   m_dragged = false;
};
