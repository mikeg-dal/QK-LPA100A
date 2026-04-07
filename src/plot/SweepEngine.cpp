#include "plot/SweepEngine.h"
#include "core/HamlibRig.h"
#include "core/LP100AProtocol.h"
#include <QTimer>
#include <cmath>

SweepEngine::SweepEngine(HamlibRig *rig, LP100AProtocol *lp100a, QObject *parent)
    : QObject(parent)
    , m_rig(rig)
    , m_lp100a(lp100a)
{
}

void SweepEngine::start(const SweepParams &params) {
    if (m_running) return;
    if (!m_rig || !m_rig->isOpen()) {
        emit sweepError("Rig not connected");
        return;
    }
    if (!m_lp100a) {
        emit sweepError("LP-100A not connected");
        return;
    }

    m_params = params;
    m_stopRequested = false;
    m_running = true;

    // Save current rig state to restore after sweep
    m_savedFreqHz = m_rig->getFrequency();
    m_savedMode = m_rig->getMode();
    emit logMessage(QString("Saved rig state: %1 Hz, mode %2")
        .arg(m_savedFreqHz, 0, 'f', 0).arg(m_savedMode));

    // Set mode and power on the rig BEFORE starting sweep
    emit logMessage(QString("Setting mode: %1").arg(m_params.txMode));
    if (!m_rig->setMode(m_params.txMode)) {
        emit logMessage("WARNING: Failed to set mode: " + m_rig->errorString());
    }

    emit logMessage(QString("Setting RF power: %1").arg(m_params.rfPower, 0, 'f', 2));
    if (!m_rig->setRFPower(m_params.rfPower)) {
        emit logMessage("WARNING: Failed to set RF power: " + m_rig->errorString());
    }

    // Use integer Hz to avoid floating point drift
    m_startHz = static_cast<qint64>(params.startMHz * 1e6);
    m_stopHz  = static_cast<qint64>(params.stopMHz * 1e6);
    m_stepHz  = static_cast<qint64>(params.stepKHz * 1000);

    m_totalSteps = static_cast<int>((m_stopHz - m_startHz) / m_stepHz) + 1;
    m_currentStep = 0;
    m_currentFreqHz = m_startHz;

    emit logMessage(QString("Sweep: %1-%2 Hz, step %3 Hz, %4 points")
        .arg(m_startHz).arg(m_stopHz).arg(m_stepHz).arg(m_totalSteps));
    emit logMessage(QString("Settle: %1ms, Sample: %2ms")
        .arg(params.settleTimeMs).arg(params.sampleTimeMs));

    nextStep();
}

void SweepEngine::stop() {
    m_stopRequested = true;
    if (m_running && m_rig && m_rig->isOpen()) {
        m_rig->setPTT(false);
        emit logMessage("Sweep stopped by user, PTT OFF");
    }
}

void SweepEngine::nextStep() {
    if (m_stopRequested || m_currentStep >= m_totalSteps) {
        m_running = false;
        m_stopRequested = false;
        if (m_rig && m_rig->isOpen()) {
            m_rig->setPTT(false);

            // Restore rig to original frequency and mode
            if (m_savedFreqHz > 0) {
                m_rig->setFrequency(m_savedFreqHz);
                emit logMessage(QString("Restored freq: %1 Hz").arg(m_savedFreqHz, 0, 'f', 0));
            }
            if (!m_savedMode.isEmpty()) {
                m_rig->setMode(m_savedMode);
                emit logMessage(QString("Restored mode: %1").arg(m_savedMode));
            }
        }
        emit logMessage("Sweep complete");
        emit sweepFinished();
        return;
    }

    double freqMHz = m_currentFreqHz / 1e6;
    emit progress(m_currentStep + 1, m_totalSteps, freqMHz);

    // Step 1: Set frequency
    emit logMessage(QString("Step %1/%2: Set freq %3 MHz (%4 Hz)")
        .arg(m_currentStep + 1).arg(m_totalSteps)
        .arg(freqMHz, 0, 'f', 3).arg(m_currentFreqHz));

    if (!m_rig->setFrequency(static_cast<double>(m_currentFreqHz))) {
        emit sweepError("Failed to set frequency: " + m_rig->errorString());
        m_running = false;
        return;
    }

    // Step 2: Wait settle time (for SteppIR, rig stabilization, etc)
    // This is the delay BEFORE transmitting
    emit logMessage(QString("  Waiting %1ms for settle...").arg(m_params.settleTimeMs));
    QTimer::singleShot(m_params.settleTimeMs, this, &SweepEngine::pttOn);
}

void SweepEngine::pttOn() {
    if (m_stopRequested) { nextStep(); return; }

    bool isCW = (m_params.txMode == "CW" || m_params.txMode == "CWR");

    if (isCW) {
        // CW mode: PTT alone doesn't produce RF — send continuous dashes
        // to key the transmitter. Each dash is ~3 dit-lengths of carrier.
        emit logMessage("  Sending CW carrier (morse dashes)");
        if (!m_rig->sendMorse("TTTTTTTTTTTTTTTTTTTT")) {
            emit logMessage("WARNING: sendMorse failed: " + m_rig->errorString());
            // Fall back to PTT in case rig handles it differently
            m_rig->setPTT(true);
        }
    } else {
        // Phone/digital modes: PTT produces carrier directly
        emit logMessage("  PTT ON");
        if (!m_rig->setPTT(true)) {
            emit sweepError("Failed to key PTT: " + m_rig->errorString());
            m_running = false;
            return;
        }
    }

    // Step 4: Wait sample time for TX to stabilize and LP-100A to measure
    emit logMessage(QString("  Sampling for %1ms...").arg(m_params.sampleTimeMs));
    QTimer::singleShot(m_params.sampleTimeMs, this, &SweepEngine::readData);
}

void SweepEngine::readData() {
    if (m_stopRequested) { nextStep(); return; }

    // Step 5: Read LP-100A measurement
    const LP100AData &data = m_lp100a->lastData();

    SweepPoint pt;
    // Round to exact kHz to avoid floating point display artifacts
    pt.freqMHz = std::round(m_currentFreqHz / 1000.0) / 1000.0;
    pt.power = data.power;
    pt.impedance = data.impedance;
    pt.phase = data.phase;
    pt.swr = data.swr;
    pt.resistance = data.resistance();
    pt.reactance = data.reactance();
    pt.dBm = data.dBm;

    emit logMessage(QString("  Read: Pwr=%1W Z=%2 Ph=%3 SWR=%4")
        .arg(pt.power, 0, 'f', 1).arg(pt.impedance, 0, 'f', 1)
        .arg(pt.phase, 0, 'f', 1).arg(pt.swr, 0, 'f', 2));

    emit pointMeasured(pt);

    // Step 6: PTT off
    pttOff();
}

void SweepEngine::pttOff() {
    emit logMessage("  PTT OFF");
    m_rig->setPTT(false);

    // Advance to next frequency (integer Hz — no floating point drift)
    m_currentStep++;
    m_currentFreqHz = m_startHz + static_cast<qint64>(m_currentStep) * m_stepHz;

    // Wait for TX to drop, then next step
    QTimer::singleShot(ReleaseTimeMs, this, &SweepEngine::nextStep);
}
