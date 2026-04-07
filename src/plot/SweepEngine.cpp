#include "plot/SweepEngine.h"
#include "core/HamlibRig.h"
#include "core/LP100AProtocol.h"
#include <QTimer>

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

    // Set mode and power on the rig BEFORE starting sweep
    emit logMessage(QString("Setting mode: %1").arg(m_params.txMode));
    if (!m_rig->setMode(m_params.txMode)) {
        emit logMessage("WARNING: Failed to set mode: " + m_rig->errorString());
    }

    emit logMessage(QString("Setting RF power: %1").arg(m_params.rfPower, 0, 'f', 2));
    if (!m_rig->setRFPower(m_params.rfPower)) {
        emit logMessage("WARNING: Failed to set RF power: " + m_rig->errorString());
    }

    // Calculate steps (adds 1 kHz margin to start/stop per LP-100A manual)
    double actualStart = params.startMHz + 0.001;
    double actualStop = params.stopMHz - 0.001;
    m_totalSteps = static_cast<int>((actualStop - actualStart) / (params.stepKHz / 1000.0)) + 1;
    m_currentStep = 0;
    m_currentFreqMHz = actualStart;

    emit logMessage(QString("Sweep: %1-%2 MHz, step %3 kHz, %4 points")
        .arg(actualStart, 0, 'f', 3).arg(actualStop, 0, 'f', 3)
        .arg(params.stepKHz).arg(m_totalSteps));
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
        }
        emit logMessage("Sweep complete");
        emit sweepFinished();
        return;
    }

    emit progress(m_currentStep + 1, m_totalSteps, m_currentFreqMHz);

    // Step 1: Set frequency
    double freqHz = m_currentFreqMHz * 1e6;
    emit logMessage(QString("Step %1/%2: Set freq %3 MHz")
        .arg(m_currentStep + 1).arg(m_totalSteps)
        .arg(m_currentFreqMHz, 0, 'f', 3));

    if (!m_rig->setFrequency(freqHz)) {
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

    // Step 3: Key PTT
    emit logMessage("  PTT ON");
    if (!m_rig->setPTT(true)) {
        emit sweepError("Failed to key PTT: " + m_rig->errorString());
        m_running = false;
        return;
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
    pt.freqMHz = m_currentFreqMHz;
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

    // Advance to next frequency
    m_currentStep++;
    m_currentFreqMHz = m_params.startMHz + 0.001 + m_currentStep * (m_params.stepKHz / 1000.0);

    // Wait for TX to drop, then next step
    QTimer::singleShot(ReleaseTimeMs, this, &SweepEngine::nextStep);
}
