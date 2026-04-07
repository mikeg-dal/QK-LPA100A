#include "core/HamlibRig.h"
#include <QDebug>
#include <QTcpSocket>
#include <QThread>
#include <hamlib/rig.h>

// Suppress ALL Hamlib debug output — redirect to /dev/null
static struct HamlibDebugInit {
    HamlibDebugInit() {
        rig_set_debug(RIG_DEBUG_NONE);
        static FILE *devnull = fopen("/dev/null", "w");
        if (devnull) rig_set_debug_file(devnull);
        rig_load_all_backends();
    }
} s_hamlibDebugInit;

// Callback for rig_list_foreach to populate the rig list
static int rigListCallback(const rig_caps *caps, void *data) {
    auto *list = static_cast<QList<HamlibRig::RigInfo> *>(data);
    if (caps && caps->rig_model != RIG_MODEL_NONE) {
        HamlibRig::RigInfo info;
        info.modelId = caps->rig_model;
        info.manufacturer = QString::fromUtf8(caps->mfg_name);
        info.model = QString::fromUtf8(caps->model_name);
        info.version = QString::fromUtf8(caps->version);
        switch (caps->status) {
            case RIG_STATUS_STABLE: info.status = "Stable"; break;
            case RIG_STATUS_BETA:   info.status = "Beta"; break;
            case RIG_STATUS_ALPHA:  info.status = "Alpha"; break;
            default:                info.status = "Unknown"; break;
        }
        list->append(info);
    }
    return 1; // continue iteration
}

HamlibRig::HamlibRig(QObject *parent)
    : QObject(parent)
{
}

HamlibRig::~HamlibRig() {
    close();
}

QList<HamlibRig::RigInfo> HamlibRig::availableRigs() {
    QList<RigInfo> rigs;
    rig_list_foreach(rigListCallback, &rigs);

    // Sort by manufacturer then model
    std::sort(rigs.begin(), rigs.end(), [](const RigInfo &a, const RigInfo &b) {
        int cmp = a.manufacturer.compare(b.manufacturer, Qt::CaseInsensitive);
        if (cmp != 0) return cmp < 0;
        return a.model.compare(b.model, Qt::CaseInsensitive) < 0;
    });

    return rigs;
}

bool HamlibRig::open(int modelId, const QString &port, int baudRate) {
    close();

    bool isNetwork = port.contains(':');

    // Pre-warm: for TCP connections, do a quick QTcpSocket connect first.
    // This triggers macOS firewall prompts via Qt's event loop, which handles
    // the permission dialog properly. Without this, Hamlib's raw socket connect
    // gets blocked by the macOS Application Firewall on first attempt.
    if (isNetwork) {
        QStringList parts = port.split(':');
        if (parts.size() == 2) {
            QTcpSocket warmup;
            warmup.connectToHost(parts[0], parts[1].toUShort());
            warmup.waitForConnected(3000);
            warmup.disconnectFromHost();
        }
    }

    m_rig = rig_init(modelId);
    if (!m_rig) {
        m_errorString = QString("Failed to init rig model %1").arg(modelId);
        emit errorOccurred(m_errorString);
        return false;
    }

    strncpy(m_rig->state.rigport.pathname, port.toUtf8().constData(),
            sizeof(m_rig->state.rigport.pathname) - 1);

    if (isNetwork) {
        m_rig->state.rigport.type.rig = RIG_PORT_NETWORK;
    }

    if (baudRate > 0) {
        m_rig->state.rigport.parm.serial.rate = baudRate;
    }

    int ret = rig_open(m_rig);

    // If first attempt fails on TCP, wait and retry once
    if (ret != RIG_OK && isNetwork) {
        rig_cleanup(m_rig);
        QThread::msleep(1000);

        m_rig = rig_init(modelId);
        if (m_rig) {
            strncpy(m_rig->state.rigport.pathname, port.toUtf8().constData(),
                    sizeof(m_rig->state.rigport.pathname) - 1);
            m_rig->state.rigport.type.rig = RIG_PORT_NETWORK;
            if (baudRate > 0)
                m_rig->state.rigport.parm.serial.rate = baudRate;
            ret = rig_open(m_rig);
        }
    }

    if (ret != RIG_OK) {
        setError(ret, QString("rig_open (model=%1 port=%2)").arg(modelId).arg(port));
        if (m_rig) { rig_cleanup(m_rig); m_rig = nullptr; }
        return false;
    }

    emit opened();
    return true;
}

void HamlibRig::close() {
    if (m_rig) {
        rig_close(m_rig);
        rig_cleanup(m_rig);
        m_rig = nullptr;
        emit closed();
    }
}

bool HamlibRig::isOpen() const {
    return m_rig != nullptr;
}

bool HamlibRig::setFrequency(double freqHz) {
    if (!m_rig) return false;
    int ret = rig_set_freq(m_rig, RIG_VFO_CURR, static_cast<freq_t>(freqHz));
    if (ret != RIG_OK) { setError(ret, "set_freq"); return false; }
    return true;
}

double HamlibRig::getFrequency() {
    if (!m_rig) return 0.0;
    freq_t freq = 0;
    int ret = rig_get_freq(m_rig, RIG_VFO_CURR, &freq);
    if (ret != RIG_OK) { setError(ret, "get_freq"); return 0.0; }
    return static_cast<double>(freq);
}

bool HamlibRig::setPTT(bool on) {
    if (!m_rig) return false;
    int ret = rig_set_ptt(m_rig, RIG_VFO_CURR, on ? RIG_PTT_ON : RIG_PTT_OFF);
    if (ret != RIG_OK) { setError(ret, "set_ptt"); return false; }
    return true;
}

bool HamlibRig::getPTT() {
    if (!m_rig) return false;
    ptt_t ptt = RIG_PTT_OFF;
    int ret = rig_get_ptt(m_rig, RIG_VFO_CURR, &ptt);
    if (ret != RIG_OK) { setError(ret, "get_ptt"); return false; }
    return ptt != RIG_PTT_OFF;
}

bool HamlibRig::setMode(const QString &mode, int passband) {
    if (!m_rig) return false;
    rmode_t rmode = rig_parse_mode(mode.toUtf8().constData());
    int ret = rig_set_mode(m_rig, RIG_VFO_CURR, rmode, static_cast<pbwidth_t>(passband));
    if (ret != RIG_OK) { setError(ret, "set_mode"); return false; }
    return true;
}

QString HamlibRig::getMode() {
    if (!m_rig) return {};
    rmode_t mode;
    pbwidth_t width;
    int ret = rig_get_mode(m_rig, RIG_VFO_CURR, &mode, &width);
    if (ret != RIG_OK) { setError(ret, "get_mode"); return {}; }
    return QString::fromUtf8(rig_strrmode(mode));
}

bool HamlibRig::sendMorse(const QString &text) {
    if (!m_rig) return false;
    int ret = rig_send_morse(m_rig, RIG_VFO_CURR, text.toUtf8().constData());
    if (ret != RIG_OK) { setError(ret, "send_morse"); return false; }
    return true;
}

bool HamlibRig::setRFPower(double level) {
    if (!m_rig) return false;
    value_t val;
    val.f = static_cast<float>(qBound(0.0, level, 1.0));
    int ret = rig_set_level(m_rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, val);
    if (ret != RIG_OK) { setError(ret, "set_rfpower"); return false; }
    return true;
}

QString HamlibRig::rigName() const {
    if (!m_rig || !m_rig->caps) return {};
    return QString("%1 %2").arg(m_rig->caps->mfg_name, m_rig->caps->model_name);
}

void HamlibRig::setError(int retcode, const QString &context) {
    m_errorString = QString("%1: %2").arg(context, QString::fromUtf8(rigerror(retcode)));
    emit errorOccurred(m_errorString);
}
