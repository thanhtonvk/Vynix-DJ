#include "controllers/midi/androidmidicontroller.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QMutexLocker>
#include <QtJniTypes>

#include "controllers/midi/midiutils.h"
#include "moc_androidmidicontroller.cpp"

Q_DECLARE_JNI_CLASS(MidiBridgeClass, "com/tondz/vynixdj/MidiBridge")

namespace {

constexpr int kMaxQueuedChunks = 4096;
// Larger SysEx messages are dropped instead of growing without bounds
constexpr int kMaxSysexLength = 64 * 1024;

constexpr char kMidiBridgeClass[] = "com/tondz/vynixdj/MidiBridge";

/// Length of a MIDI message including the status byte, or 0 for SysEx.
int messageLength(unsigned char status) {
    if (status < 0xF0) {
        const unsigned char opCode = status & 0xF0;
        return (opCode == 0xC0 || opCode == 0xD0) ? 2 : 3;
    }
    switch (status) {
    case 0xF0: // SysEx start, variable length
        return 0;
    case 0xF1: // MIDI time code quarter frame
    case 0xF3: // Song select
        return 2;
    case 0xF2: // Song position pointer
        return 3;
    default: // Tune request, EOX, real-time and undefined messages
        return 1;
    }
}

} // namespace

// JNI callbacks must be declared at global scope for Q_DECLARE_JNI_NATIVE_METHOD
static void nativeOnMidiData(JNIEnv* env,
        jclass,
        jlong nativeController,
        jbyteArray data,
        jint offset,
        jint count,
        jlong timestamp) {
    auto* pController = reinterpret_cast<AndroidMidiController*>(nativeController);
    if (!pController || count <= 0) {
        return;
    }
    QByteArray buffer(count, Qt::Uninitialized);
    env->GetByteArrayRegion(data, offset, count, reinterpret_cast<jbyte*>(buffer.data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return;
    }
    pController->enqueueData(buffer.constData(), count, timestamp);
}
Q_DECLARE_JNI_NATIVE_METHOD(nativeOnMidiData)

// static
bool AndroidMidiController::registerNativeMethods() {
    QJniEnvironment env;
    return env.registerNativeMethods<QtJniTypes::MidiBridgeClass>({
            Q_JNI_NATIVE_METHOD(nativeOnMidiData),
    });
}

AndroidMidiController::AndroidMidiController(int deviceId,
        const QString& name,
        const QString& manufacturer,
        const QString& serialNumber,
        bool isUsb,
        bool hasInput,
        bool hasOutput)
        : MidiController(name),
          m_deviceId(deviceId),
          m_manufacturer(manufacturer),
          m_serialNumber(serialNumber),
          m_isUsb(isUsb),
          m_status(0),
          m_data{0, 0},
          m_dataCount(0),
          m_expectedLength(0),
          m_inSysex(false) {
    setInputDevice(hasInput);
    setOutputDevice(hasOutput);
}

AndroidMidiController::~AndroidMidiController() {
    if (isOpen()) {
        close();
    }
}

int AndroidMidiController::open(const QString& resourcePath) {
    if (isOpen()) {
        qCWarning(m_logBase) << "Android MIDI device" << getName() << "already open";
        return -1;
    }

    resetParser();
    {
        const auto locker = QMutexLocker(&m_queueMutex);
        m_queue.clear();
    }

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    m_bridge = QJniObject::callStaticObjectMethod(kMidiBridgeClass,
            "open",
            "(Landroid/content/Context;IJ)Lcom/tondz/vynixdj/MidiBridge;",
            context.object(),
            static_cast<jint>(m_deviceId),
            reinterpret_cast<jlong>(this));
    QJniEnvironment env;
    if (env.checkAndClearExceptions() || !m_bridge.isValid()) {
        qCWarning(m_logBase) << "Failed to open Android MIDI device" << getName();
        m_bridge = QJniObject();
        return -2;
    }
    qCInfo(m_logBase) << "Opened Android MIDI device" << getName()
                      << "input:" << m_bridge.callMethod<jboolean>("hasInput")
                      << "output:" << m_bridge.callMethod<jboolean>("hasOutput");

    startEngine();
    // Mark the device as open before applying the mapping, otherwise the
    // initial LED state sent by the output handlers is dropped.
    setOpen(true);
    applyMapping(resourcePath);
    return 0;
}

int AndroidMidiController::close() {
    if (!isOpen()) {
        qCWarning(m_logBase) << "Android MIDI device" << getName() << "already closed";
        return -1;
    }

    stopEngine();
    MidiController::close();

    if (m_bridge.isValid()) {
        // Detaches the Java receiver from this object before closing the ports
        m_bridge.callMethod<void>("close");
        QJniEnvironment env;
        env.checkAndClearExceptions();
        m_bridge = QJniObject();
    }
    {
        const auto locker = QMutexLocker(&m_queueMutex);
        m_queue.clear();
    }
    resetParser();

    setOpen(false);
    return 0;
}

void AndroidMidiController::enqueueData(
        const char* pData, int count, qint64 timestampNanos) {
    const auto locker = QMutexLocker(&m_queueMutex);
    if (m_queue.size() >= kMaxQueuedChunks) {
        // The controller thread is not keeping up, drop the oldest data
        m_queue.removeFirst();
    }
    m_queue.append(Chunk{QByteArray(pData, count), timestampNanos});
}

bool AndroidMidiController::poll() {
    {
        const auto locker = QMutexLocker(&m_queueMutex);
        if (m_queue.isEmpty()) {
            return false;
        }
        m_pending.swap(m_queue);
    }
    for (const Chunk& chunk : std::as_const(m_pending)) {
        const auto timestamp = mixxx::Duration::fromNanos(chunk.timestampNanos);
        for (const char byte : chunk.data) {
            parseByte(static_cast<unsigned char>(byte), timestamp);
        }
    }
    m_pending.clear();
    return true;
}

void AndroidMidiController::resetParser() {
    m_status = 0;
    m_dataCount = 0;
    m_expectedLength = 0;
    m_inSysex = false;
    m_sysex.clear();
}

void AndroidMidiController::parseByte(unsigned char byte, mixxx::Duration timestamp) {
    // Real-time messages may appear anywhere, even inside SysEx, and do not
    // affect the running status.
    if (byte >= 0xF8) {
        receivedShortMessage(byte, 0, 0, timestamp);
        return;
    }

    if (m_inSysex) {
        if (byte == MidiUtils::opCodeValue(MidiOpCode::EndOfExclusive)) {
            m_sysex.append(static_cast<char>(byte));
            m_inSysex = false;
            receive(m_sysex, timestamp);
            m_sysex.clear();
            return;
        }
        if (byte < 0x80) {
            if (m_sysex.size() < kMaxSysexLength) {
                m_sysex.append(static_cast<char>(byte));
            }
            return;
        }
        // A status byte terminates an unfinished SysEx message
        qCWarning(m_logInput) << "Buggy MIDI device: SysEx interrupted!";
        m_inSysex = false;
        m_sysex.clear();
    }

    if (byte & 0x80) {
        // Status byte
        if (byte == 0xF0) {
            m_inSysex = true;
            m_sysex.clear();
            m_sysex.append(static_cast<char>(byte));
            m_status = 0;
            return;
        }
        const int length = messageLength(byte);
        if (length == 1) {
            // System common messages without data cancel the running status
            m_status = 0;
            receivedShortMessage(byte, 0, 0, timestamp);
            return;
        }
        m_status = byte;
        m_expectedLength = length;
        m_dataCount = 0;
        return;
    }

    // Data byte
    if (m_status == 0) {
        // No status received yet (or after a system message), ignore
        return;
    }
    m_data[m_dataCount++] = byte;
    if (m_dataCount + 1 < m_expectedLength) {
        return;
    }
    receivedShortMessage(m_status, m_data[0], m_dataCount > 1 ? m_data[1] : 0, timestamp);
    m_dataCount = 0;
    if (m_status >= 0xF0) {
        // Running status only applies to channel messages
        m_status = 0;
    }
}

void AndroidMidiController::sendShortMsg(
        unsigned char status, unsigned char byte1, unsigned char byte2) {
    const int length = messageLength(status);
    if (length == 0) {
        return;
    }
    QByteArray message;
    message.append(static_cast<char>(status));
    if (length > 1) {
        message.append(static_cast<char>(byte1));
    }
    if (length > 2) {
        message.append(static_cast<char>(byte2));
    }
    if (sendBytes(message)) {
        qCDebug(m_logOutput) << QStringLiteral("outgoing: ")
                             << MidiUtils::formatMidiOpCode(getName(),
                                        status,
                                        byte1,
                                        byte2,
                                        MidiUtils::channelFromStatus(status),
                                        MidiUtils::opCodeFromStatus(status));
    }
}

bool AndroidMidiController::sendBytes(const QByteArray& data) {
    if (!m_bridge.isValid() || data.isEmpty()) {
        return false;
    }
    QJniEnvironment env;
    jbyteArray array = env->NewByteArray(static_cast<jsize>(data.size()));
    if (!array) {
        env.checkAndClearExceptions();
        return false;
    }
    env->SetByteArrayRegion(array,
            0,
            static_cast<jsize>(data.size()),
            reinterpret_cast<const jbyte*>(data.constData()));
    const bool sent = m_bridge.callMethod<jboolean>("send", "([B)Z", array);
    env->DeleteLocalRef(array);
    if (env.checkAndClearExceptions() || !sent) {
        qCWarning(m_logOutput) << "Error sending MIDI data to" << getName();
        return false;
    }
    return true;
}
