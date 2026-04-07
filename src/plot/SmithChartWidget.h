#pragma once

#include <QWidget>
#include <QPainter>
#include <QVector>
#include "plot/SweepData.h"

// Custom QPainter Smith Chart widget.
// Draws constant-R circles and constant-X arcs on the reflection coefficient plane.
// Overlay measured impedance data as a connected curve.
class SmithChartWidget : public QWidget {
    Q_OBJECT
public:
    explicit SmithChartWidget(QWidget *parent = nullptr);

    void setData(const SweepData *data) { m_data = data; update(); }
    void clearData() { m_data = nullptr; update(); }

    // Reference impedance (typically 50 ohms)
    void setZ0(double z0) { m_z0 = z0; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // Convert normalized impedance (r + jx) to pixel coordinates
    QPointF impedanceToPixel(double r, double x) const;

    // Draw the Smith Chart grid
    void drawGrid(QPainter &p) const;
    void drawConstantR(QPainter &p, double r) const;
    void drawConstantX(QPainter &p, double x) const;
    void drawLabels(QPainter &p) const;
    void drawData(QPainter &p) const;

    // Chart geometry (computed in paintEvent from widget size)
    mutable QPointF m_center;
    mutable double m_radius = 0;

    const SweepData *m_data = nullptr;
    double m_z0 = 50.0;  // Reference impedance
};
