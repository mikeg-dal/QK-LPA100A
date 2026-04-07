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

QVector<double> SweepData::polyFit(const QVector<double> &x, const QVector<double> &y, int order) {
    int n = x.size();
    int m = order + 1;
    QVector<double> coeffs(m, 0.0);

    if (n < m) return coeffs;

    // Build augmented matrix for normal equations (Vandermonde approach)
    // Matrix is m x (m+1)
    QVector<QVector<double>> mat(m, QVector<double>(m + 1, 0.0));

    for (int row = 0; row < m; ++row) {
        for (int col = 0; col < m; ++col) {
            double sum = 0;
            for (int i = 0; i < n; ++i)
                sum += std::pow(x[i], row + col);
            mat[row][col] = sum;
        }
        double sum = 0;
        for (int i = 0; i < n; ++i)
            sum += y[i] * std::pow(x[i], row);
        mat[row][m] = sum;
    }

    // Gaussian elimination with partial pivoting
    for (int col = 0; col < m; ++col) {
        // Find pivot
        int maxRow = col;
        for (int row = col + 1; row < m; ++row) {
            if (std::abs(mat[row][col]) > std::abs(mat[maxRow][col]))
                maxRow = row;
        }
        std::swap(mat[col], mat[maxRow]);

        if (std::abs(mat[col][col]) < 1e-12) continue;

        // Eliminate below
        for (int row = col + 1; row < m; ++row) {
            double factor = mat[row][col] / mat[col][col];
            for (int j = col; j <= m; ++j)
                mat[row][j] -= factor * mat[col][j];
        }
    }

    // Back substitution
    for (int row = m - 1; row >= 0; --row) {
        coeffs[row] = mat[row][m];
        for (int col = row + 1; col < m; ++col)
            coeffs[row] -= mat[row][col] * coeffs[col];
        if (std::abs(mat[row][row]) > 1e-12)
            coeffs[row] /= mat[row][row];
    }

    return coeffs;
}

bool SweepData::exportCsv(const QString &path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "Freq_MHz,Power_W,Z_Ohms,Phase_Deg,R_Ohms,X_Ohms,SWR,dBm,RL_dB,Gamma\n";

    for (const auto &pt : m_points) {
        out << QString::number(pt.freqMHz, 'f', 3) << ','
            << QString::number(pt.power, 'f', 2) << ','
            << QString::number(pt.impedance, 'f', 1) << ','
            << QString::number(pt.phase, 'f', 1) << ','
            << QString::number(pt.resistance, 'f', 1) << ','
            << QString::number(pt.reactance, 'f', 1) << ','
            << QString::number(pt.swr, 'f', 2) << ','
            << QString::number(pt.dBm, 'f', 1) << ','
            << QString::number(returnLoss(pt.swr), 'f', 2) << ','
            << QString::number(reflectionCoeff(pt.swr), 'f', 4) << '\n';
    }

    return true;
}
