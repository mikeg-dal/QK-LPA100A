#include "plot/SweepData.h"
#include <QFile>
#include <QTextStream>
#include <cmath>

void SweepData::flipSign(int i) {
    if (i < 0 || i >= m_points.size()) return;
    auto &pt = m_points[i];
    pt.reactance = -pt.reactance;
    pt.phase = -pt.phase;
    pt.signFlipped = !pt.signFlipped;
}

void SweepData::autoCorrectSigns() {
    // Sign detection algorithm: reactance/phase should change smoothly.
    // If the slope of X reverses abruptly (sign of X flips between adjacent
    // points without crossing through zero smoothly), flip the offending point.
    // This matches the LP-100A Plot program's detection algorithm.

    if (m_points.size() < 3) return;

    for (int i = 1; i < m_points.size() - 1; ++i) {
        double xPrev = m_points[i - 1].reactance;
        double xCurr = m_points[i].reactance;
        double xNext = m_points[i + 1].reactance;

        // Check if current point is an outlier — sign is opposite to both neighbors
        bool prevSameSign = (xPrev * xCurr) >= 0;
        bool nextSameSign = (xCurr * xNext) >= 0;
        bool neighborsAgree = (xPrev * xNext) >= 0;

        // If neighbors agree but current disagrees with both, flip it
        if (!prevSameSign && !nextSameSign && neighborsAgree) {
            flipSign(i);
        }
    }
}

double SweepData::returnLoss(double swr) {
    if (swr <= 1.0) return 999.0; // Perfect match
    double gamma = (swr - 1.0) / (swr + 1.0);
    return -20.0 * std::log10(gamma);
}

double SweepData::reflectionCoeff(double swr) {
    if (swr <= 1.0) return 0.0;
    return (swr - 1.0) / (swr + 1.0);
}

bool SweepData::exportCsv(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "Freq_MHz,Power_W,Z_Ohms,Phase_Deg,R_Ohms,X_Ohms,SWR,dBm,RL_dB,Gamma\n";

    for (const auto &pt : m_points) {
        out << pt.freqMHz << ','
            << pt.power << ','
            << pt.impedance << ','
            << pt.phase << ','
            << pt.resistance << ','
            << pt.reactance << ','
            << pt.swr << ','
            << pt.dBm << ','
            << returnLoss(pt.swr) << ','
            << reflectionCoeff(pt.swr) << '\n';
    }

    return true;
}
