#pragma once

#include <QVector>
#include <QString>

// Single measurement point from a frequency sweep
struct SweepPoint {
    double freqMHz = 0.0;
    double power = 0.0;
    double impedance = 0.0;   // Z magnitude (ohms)
    double phase = 0.0;       // Phase angle (degrees)
    double swr = 1.0;
    double resistance = 0.0;  // R = Z * cos(phase)
    double reactance = 0.0;   // X = Z * sin(phase) — sign may be corrected
    double dBm = 0.0;
    bool signFlipped = false;  // User manually flipped the sign of X/phase
};

// Container for sweep results
class SweepData {
public:
    void clear() { m_points.clear(); }
    void addPoint(const SweepPoint &pt) { m_points.append(pt); }

    int count() const { return m_points.size(); }
    bool isEmpty() const { return m_points.isEmpty(); }

    const SweepPoint &at(int i) const { return m_points.at(i); }
    SweepPoint &operator[](int i) { return m_points[i]; }
    const QVector<SweepPoint> &points() const { return m_points; }

    // Flip the sign of reactance/phase at index i
    void flipSign(int i);

    // Auto-detect and correct sign based on slope analysis
    void autoCorrectSigns();

    // Return loss: RL = -20 * log10(|gamma|) where gamma = (SWR-1)/(SWR+1)
    static double returnLoss(double swr);

    // Reflection coefficient: gamma = (SWR-1)/(SWR+1)
    static double reflectionCoeff(double swr);

    // Export to CSV
    bool exportCsv(const QString &path) const;

private:
    QVector<SweepPoint> m_points;
};
