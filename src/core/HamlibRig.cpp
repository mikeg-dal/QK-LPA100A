#include "core/HamlibRig.h"
#include <QDebug>
#include <hamlib/rig.h>

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
    rig_set_debug(RIG_DEBUG_NONE); // Suppress Hamlib debug output
}

HamlibRig::~HamlibRig() {
    close();
}

QList<HamlibRig::RigInfo> HamlibRig::availableRigs() {
    QList<RigInfo> rigs;
    rig_load_all_backends();
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

    m_rig = rig_init(modelId);
    if (!m_rig) {
        m_errorString = QString("Failed to init rig model %1").arg(modelId);
        emit errorOccurred(m_errorString);
        return false;
    }

    // Set the port (serial device path or hostname:port for network rigs)
    strncpy(m_rig->state.rigport.pathname, port.toUtf8().constData(),
            sizeof(m_rig->state.rigport.pathname) - 1);

    if (baudRate > 0) {
        m_rig->state.rigport.parm.serial.rate = baudRate;
    }

    int ret = rig_open(m_rig);
    if (ret != RIG_OK) {
        setError(ret, "rig_open");
        rig_cleanup(m_rig);
        m_rig = nullptr;
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

QString HamlibRig::rigName() const {
    if (!m_rig || !m_rig->caps) return {};
    return QString("%1 %2").arg(m_rig->caps->mfg_name, m_rig->caps->model_name);
}

void HamlibRig::setError(int retcode, const QString &context) {
    m_errorString = QString("%1: %2").arg(context, QString::fromUtf8(rigerror(retcode)));
    emit errorOccurred(m_errorString);
}
