#pragma once
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QSpinBox>
#include <limits>

namespace utility_qt {

QSpinBox* createSpinBox(bool readonly = false, bool noButtonSymbol = false);

QSpinBox* createHexSpinBox(bool readonly = false, bool noButtonSymbol = false);

QGraphicsDropShadowEffect* createGraphicsShadow(
    QWidget* widget,
    const QColor& color = QColor(105, 105, 105, 200),
    int blurRadius = 9);

bool createFile(const QString& path);

void calcStartEndPoint(const QRectF& rect,
                       qreal angle,
                       QPointF& start,
                       QPointF& end);

void paintWin10Progress(QPaintDevice* device, const QColor& color, qreal t_);

void paintBeautifulProgress(QPaintDevice* device,
                            const double A,
                            const double k,
                            const QPointF& midPos,
                            const QColor& color1,
                            const QColor& color2,
                            const QColor& color3,
                            const QColor& color4);

QPoint posInParentCenter(QWidget* child, QWidget* parent);

QByteArray getFileMD5Hash(const QString& filePath);

// 找到它的窗口树中最接近的一层的QScrollArea
QScrollArea* findScrollArea(QWidget* who);

}  // namespace utility_qt
