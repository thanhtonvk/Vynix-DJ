#pragma once

#include <QByteArray>
#include <QJniObject>
#include <QMutex>
#include <QVector>

#include "controllers/midi/midicontroller.h"

/// MIDI controller backed by the Android MIDI API (android.media.midi).
///
/// Device access is implemented in Java (com.tondz.vynixdj.MidiBridge). Data
/// from the device arrives on an Android binder thread and is queued here,
/// then parsed into MIDI messages from poll() on the controller thread.
class AndroidMidiController : public MidiController {
    Q_OBJECT
  public:
    AndroidMidiController(int deviceId,
            const QString& name,
            const QString& manufacturer,
            const QString& serialNumber,
            bool isUsb,
            bool hasInput,
            bool hasOutput);
    ~AndroidMidiController() override;

    PhysicalTransportProtocol getPhysicalTransportProtocol() const override {
        return m_isUsb ? PhysicalTransportProtocol::USB : PhysicalTransportProtocol::UNKNOWN;
    }
    QString getVendorString() const override {
        return m_manufacturer;
    }
    QString getProductString() const override {
        return getName();
    }
    std::optional<uint16_t> getVendorId() const override {
        return std::nullopt;
    }
    std::optional<uint16_t> getProductId() const override {
        return std::nullopt;
    }
    QString getSerialNumber() const override {
        return m_serialNumber;
    }
    std::optional<uint8_t> getUsbInterfaceNumber() const override {
        return std::nullopt;
    }

    int deviceId() const {
        return m_deviceId;
    }

    /// Called from the Java receiver thread with raw MIDI bytes.
    void enqueueData(const char* pData, int count, qint64 timestampNanos);

    /// Registers the native callback of the Java MidiBridge class. Must be
    /// called once before any controller is opened.
    static bool registerNativeMethods();

  private slots:
    bool poll() override;

  protected:
    void sendShortMsg(unsigned char status,
            unsigned char byte1,
            unsigned char byte2) override;

  private:
    struct Chunk {
        QByteArray data;
        qint64 timestampNanos;
    };

    int open(const QString& resourcePath) override;
    int close() override;
    bool sendBytes(const QByteArray& data) override;
    bool isPolling() const override {
        return true;
    }

    void parseByte(unsigned char byte, mixxx::Duration timestamp);
    void resetParser();

    const int m_deviceId;
    const QString m_manufacturer;
    const QString m_serialNumber;
    const bool m_isUsb;

    QJniObject m_bridge;

    QMutex m_queueMutex;
    QVector<Chunk> m_queue;
    // Only used on the controller thread in poll()
    QVector<Chunk> m_pending;

    // MIDI stream parser state
    unsigned char m_status;
    unsigned char m_data[2];
    int m_dataCount;
    int m_expectedLength;
    bool m_inSysex;
    QByteArray m_sysex;
};
