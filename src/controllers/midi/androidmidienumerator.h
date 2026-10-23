#pragma once

#include "controllers/midi/midienumerator.h"

/// Discovers MIDI controllers through the Android MIDI API.
class AndroidMidiEnumerator : public MidiEnumerator {
    Q_OBJECT
  public:
    AndroidMidiEnumerator();
    ~AndroidMidiEnumerator() override;

    QList<Controller*> queryDevices() override;

  private:
    QList<Controller*> m_devices;
};
