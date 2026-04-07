#include "plot/SmithChartWidget.h"
#include "Style.h"
#include <QPainterPath>
#include <cmath>

// Normalized R values for constant-resistance circles
static constexpr double kRValues[] = {
    0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 10.0, 20.0, 50.0
};
static constexpr int kNumR = sizeof(kRValues) / sizeof(kRValues[0]);

// Normalized X values for constant-reactance arcs (positive only; negative mirrored)
static constexpr double kXValues[] = {
    0.2, 0.4, 0.6, 0.8, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 10.0, 20.0, 50.0
};
static constexpr int kNumX = sizeof(kXValues) / sizeof(kXValues[0]);

SmithChartWidget::SmithChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

QPointF SmithChartWidget::impedanceToPixel(double r, double x) const {
    // Normalize by Z0
    double rn = r / m_z0;
    double xn = x / m_z0;

    // Möbius transform: z → Γ
    double denom = (rn + 1.0) * (rn + 1.0) + xn * xn;
    double gr = (rn * rn + xn * xn - 1.0) / denom;
    double gi = (2.0 * xn) / denom;

    // Γ-plane to pixels (y inverted for Qt)
    return QPointF(m_center.x() + gr * m_radius,
                   m_center.y() - gi * m_radius);
}

void SmithChartWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Compute geometry from current widget size
    int w = width();
    int h = height();
    int titleHeight = 24;
    int chartH = h - titleHeight;
    m_center = QPointF(w / 2.0, titleHeight + chartH / 2.0);
    m_radius = qMin(w, chartH) / 2.0 * 0.82; // 18% margin for labels

    // Background
    p.fillRect(rect(), QColor(Style::Color::DarkBackground));

    // Title
    QFont titleFont(Style::Font::Family);
    titleFont.setPixelSize(14);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(QColor(Style::Color::TextWhite));
    p.drawText(QRect(0, 0, w, titleHeight), Qt::AlignCenter, "Smith Chart");

    // Draw grid (clipped to unit circle)
    drawGrid(p);

    // Draw data overlay
    if (m_data && !m_data->isEmpty()) {
        drawData(p);
    }
}

void SmithChartWidget::drawGrid(QPainter &p) const {
    // Set up clipping to unit circle
    QPainterPath clipPath;
    clipPath.addEllipse(m_center, m_radius, m_radius);
    p.setClipPath(clipPath);

    // --- Constant-R circles ---
    for (int i = 0; i < kNumR; ++i) {
        drawConstantR(p, kRValues[i]);
    }

    // --- Constant-X arcs ---
    for (int i = 0; i < kNumX; ++i) {
        drawConstantX(p, kXValues[i]);
    }

    // --- Horizontal axis (X=0 line) ---
    p.setPen(QPen(QColor(Style::Color::PanelBorder), 1));
    p.drawLine(QPointF(m_center.x() - m_radius, m_center.y()),
               QPointF(m_center.x() + m_radius, m_center.y()));

    // Remove clipping for border and labels
    p.setClipping(false);

    // --- Unit circle border ---
    p.setPen(QPen(QColor(Style::Color::TextGray), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(m_center, m_radius, m_radius);

    // --- Labels ---
    drawLabels(p);
}

void SmithChartWidget::drawConstantR(QPainter &p, double r) const {
    double centerGr = r / (r + 1.0);
    double radiusG  = 1.0 / (r + 1.0);

    // Convert Γ-plane circle to pixel bounding rect
    double left = m_center.x() + (centerGr - radiusG) * m_radius;
    double top  = m_center.y() - radiusG * m_radius;
    double diam = 2.0 * radiusG * m_radius;

    // R=1.0 circle (center impedance) gets a slightly thicker line
    double lineWidth = (r == 1.0) ? 1.2 : 0.7;
    p.setPen(QPen(QColor(Style::Color::PanelBorder), lineWidth));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(left, top, diam, diam));
}

void SmithChartWidget::drawConstantX(QPainter &p, double x) const {
    double radiusG = 1.0 / x;

    p.setPen(QPen(QColor(Style::Color::PanelBorder), 0.5));
    p.setBrush(Qt::NoBrush);

    // Positive X (upper half — inductive)
    {
        double centerGr = 1.0;
        double centerGi = 1.0 / x;
        double left = m_center.x() + (centerGr - radiusG) * m_radius;
        double top  = m_center.y() - (centerGi + radiusG) * m_radius;
        double diam = 2.0 * radiusG * m_radius;
        p.drawEllipse(QRectF(left, top, diam, diam));
    }

    // Negative X (lower half — capacitive)
    {
        double centerGr = 1.0;
        double centerGi = -1.0 / x;
        double left = m_center.x() + (centerGr - radiusG) * m_radius;
        double top  = m_center.y() - (centerGi + radiusG) * m_radius;
        double diam = 2.0 * radiusG * m_radius;
        p.drawEllipse(QRectF(left, top, diam, diam));
    }
}

void SmithChartWidget::drawLabels(QPainter &p) const {
    QFont labelFont(Style::Font::Family);
    labelFont.setPixelSize(Style::Font::Tiny);
    p.setFont(labelFont);
    p.setPen(QColor(Style::Color::TextGray));

    // R labels along horizontal axis
    // Placed at left intersection of each R-circle with real axis: Γr = (r-1)/(r+1)
    for (int i = 0; i < kNumR; ++i) {
        double r = kRValues[i];
        if (r == 0) continue;
        double gr = (r - 1.0) / (r + 1.0);
        double px = m_center.x() + gr * m_radius;
        double py = m_center.y();

        QString label = QString::number(r, 'g', 3);
        QRectF textRect(px - 12, py + 2, 24, 12);
        p.drawText(textRect, Qt::AlignCenter, label);
    }

    // X labels along outer unit circle perimeter
    // Placed where each X-arc intersects the unit circle
    double labelOffset = 10;
    for (int i = 0; i < kNumX; ++i) {
        double x = kXValues[i];
        double gr = (x * x - 1.0) / (x * x + 1.0);
        double gi = 2.0 * x / (x * x + 1.0);

        // Positive X label (upper half)
        {
            double px = m_center.x() + gr * (m_radius + labelOffset);
            double py = m_center.y() - gi * (m_radius + labelOffset);
            QString label = QString::number(x, 'g', 3);
            p.drawText(QPointF(px - 8, py + 3), label);
        }

        // Negative X label (lower half)
        {
            double px = m_center.x() + gr * (m_radius + labelOffset);
            double py = m_center.y() + gi * (m_radius + labelOffset);
            QString label = "-" + QString::number(x, 'g', 3);
            p.drawText(QPointF(px - 10, py + 3), label);
        }
    }
}

void SmithChartWidget::drawData(QPainter &p) const {
    if (!m_data || m_data->isEmpty()) return;

    // Build polyline from impedance data
    QVector<QPointF> points;
    points.reserve(m_data->count());

    for (int i = 0; i < m_data->count(); ++i) {
        const SweepPoint &pt = m_data->at(i);
        QPointF pixel = impedanceToPixel(pt.resistance, pt.reactance);

        // Only include if within unit circle (passive load)
        double dx = pixel.x() - m_center.x();
        double dy = pixel.y() - m_center.y();
        if (dx * dx + dy * dy <= m_radius * m_radius * 1.01) {
            points.append(pixel);
        }
    }

    if (points.isEmpty()) return;

    // Draw the impedance curve (red, matching LP-100A reference)
    p.setPen(QPen(QColor(Style::Color::StatusRed), 2.5));
    p.setBrush(Qt::NoBrush);
    if (points.size() > 1) {
        p.drawPolyline(points.data(), points.size());
    }

    // Start frequency marker (green dot)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Style::Color::StatusGreen));
    p.drawEllipse(points.first(), 4, 4);

    // End frequency marker
    p.setBrush(QColor(Style::Color::AccentAmber));
    p.drawEllipse(points.last(), 4, 4);
}
