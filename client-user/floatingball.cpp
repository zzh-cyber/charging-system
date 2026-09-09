#include "floatingball.h"

#include <QMouseEvent>
#include <QPainter>

FloatingBall::FloatingBall(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(110, 110);
    setCursor(Qt::PointingHandCursor);
}

void FloatingBall::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 优先绘制自定义图片（按比例缩放、居中裁切，填满整个圆球）
    const QPixmap pm(QStringLiteral(":/images/ai_avatar.png"));
    if (!pm.isNull()) {
        const QPixmap scaled = pm.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                         Qt::SmoothTransformation);
        const int x = (scaled.width() - width()) / 2;
        const int y = (scaled.height() - height()) / 2;
        p.drawPixmap(-x, -y, scaled);
        return;
    }

    // 兜底：图片缺失时画荧光绿圆 + 小充 文字
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(QStringLiteral("#74EC8B")));
    p.drawEllipse(rect());

    QFont f = font();
    f.setPixelSize(40);
    f.setBold(true);
    p.setFont(f);
    p.setPen(QColor(QStringLiteral("#171D27")));
    p.drawText(rect(), Qt::AlignCenter, QStringLiteral("小充"));
}

void FloatingBall::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressPos = event->pos();
        m_dragged = false;
    }
}

void FloatingBall::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(pos() + event->pos() - m_pressPos);
        m_dragged = true;
    }
}

void FloatingBall::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && !m_dragged)
        emit clicked();
    m_dragged = false;
}
