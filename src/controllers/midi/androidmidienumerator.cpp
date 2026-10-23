#include "controllers/midi/androidmidienumerator.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QVector>

#include "controllers/midi/androidmidicontroller.h"
#include "moc_androidmidienumerator.cpp"

namespace {

constexpr char kMidiBridgeClass[] = "com/tondz/vynixdj/MidiBridge";
// android.media.midi.MidiDeviceInfo.TYPE_USB
constexpr jint kMidiDeviceTypeUsb = 1;

QString deviceString(const QJniObject& context, const char* method, int deviceId) {
    return QJniObject::callStaticObjectMethod(kMidiBridgeClass,
            method,
            "(Landroid/content/Context;I)Ljava/lang/String;",
            context.object(),
            static_cast<jint>(deviceId))
            .toString();
}

int deviceInt(const QJniObject& context, const char* method, int deviceId) {
    return QJniObject::callStaticMethod<jint>(kMidiBridgeClass,
            method,
            "(Landroid/content/Context;I)I",
            context.object(),
            static_cast<jint>(deviceId));
}

} // namespace

AndroidMidiEnumerator::AndroidMidiEnumerator() {
    static const bool s_nativesRegistered = AndroidMidiController::registerNativeMethods();
    if (!s_nativesRegistered) {
        qWarning() << "Failed to register the Android MIDI native callback";
    }
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniObject::callStaticMethod<void>(kMidiBridgeClass,
                "registerDeviceCallback",
                "(Landroid/content/Context;)V",
                context.object());
        QJniEnvironment env;
        env.checkAndClearExceptions();
    }
}

AndroidMidiEnumerator::~AndroidMidiEnumerator() {
    qDebug() << "Deleting Android MIDI devices...";
    qDeleteAll(m_devices);
    m_devices.clear();
}

QList<Controller*> AndroidMidiEnumerator::queryDevices() {
    qDebug() << "Scanning Android MIDI devices:";

    qDeleteAll(m_devices);
    m_devices.clear();

    QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid()) {
        qWarning() << "No Android context, cannot enumerate MIDI devices";
        return m_devices;
    }

    const QJniObject idArray = QJniObject::callStaticObjectMethod(kMidiBridgeClass,
            "getDeviceIds",
            "(Landroid/content/Context;)[I",
            context.object());
    QJniEnvironment env;
    if (env.checkAndClearExceptions() || !idArray.isValid()) {
        qWarning() << "Failed to enumerate Android MIDI devices";
        return m_devices;
    }
    const auto jIds = idArray.object<jintArray>();
    QVector<jint> deviceIds(env->GetArrayLength(jIds));
    env->GetIntArrayRegion(jIds, 0, deviceIds.size(), deviceIds.data());

    for (const jint deviceId : deviceIds) {
        // Device output ports send data to the app, input ports receive it
        const int outputPorts = deviceInt(context, "getOutputPortCount", deviceId);
        const int inputPorts = deviceInt(context, "getInputPortCount", deviceId);
        if (outputPorts <= 0 && inputPorts <= 0) {
            continue;
        }
        const QString name = deviceString(context, "getDeviceName", deviceId);
        const QString manufacturer = deviceString(context, "getManufacturer", deviceId);
        const QString serialNumber = deviceString(context, "getSerialNumber", deviceId);
        const bool isUsb = deviceInt(context, "getDeviceType", deviceId) == kMidiDeviceTypeUsb;
        env.checkAndClearExceptions();

        qDebug() << " Found MIDI device" << deviceId << name
                 << "outputs:" << outputPorts << "inputs:" << inputPorts
                 << (isUsb ? "(USB)" : "");
        m_devices.push_back(new AndroidMidiController(deviceId,
                name,
                manufacturer,
                serialNumber,
                isUsb,
                outputPorts > 0,
                inputPorts > 0));
    }
    return m_devices;
}
