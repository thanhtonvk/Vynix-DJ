package com.tondz.vynixdj;

import android.content.Context;
import android.media.midi.MidiDevice;
import android.media.midi.MidiDeviceInfo;
import android.media.midi.MidiInputPort;
import android.media.midi.MidiManager;
import android.media.midi.MidiOutputPort;
import android.media.midi.MidiReceiver;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/// Bridge between the Android MIDI API and the native AndroidMidiController.
///
/// Port naming follows the Android API: an output port of the device sends
/// data to the app, an input port of the device receives data from the app.
public class MidiBridge {
    private static final String TAG = "VynixMidi";
    private static final long OPEN_TIMEOUT_MS = 5000;

    private static HandlerThread sHandlerThread;
    private static Handler sHandler;

    private static boolean sDeviceCallbackRegistered = false;
    private static final Set<Integer> sKnownDeviceIds = new HashSet<>();

    private final MidiDevice mDevice;
    private final List<MidiOutputPort> mOutputPorts;
    private final MidiInputPort mInputPort;
    private final Receiver mReceiver;

    private static native void nativeOnMidiData(
            long nativeController, byte[] data, int offset, int count, long timestamp);

    private static synchronized Handler handler() {
        if (sHandler == null) {
            sHandlerThread = new HandlerThread("VynixMidi");
            sHandlerThread.start();
            sHandler = new Handler(sHandlerThread.getLooper());
        }
        return sHandler;
    }

    private static MidiManager midiManager(Context context) {
        if (context == null) {
            return null;
        }
        return (MidiManager) context.getSystemService(Context.MIDI_SERVICE);
    }

    @SuppressWarnings("deprecation")
    private static MidiDeviceInfo[] devices(Context context) {
        MidiManager manager = midiManager(context);
        if (manager == null) {
            return new MidiDeviceInfo[0];
        }
        return manager.getDevices();
    }

    private static MidiDeviceInfo findDevice(Context context, int deviceId) {
        for (MidiDeviceInfo info : devices(context)) {
            if (info.getId() == deviceId) {
                return info;
            }
        }
        return null;
    }

    /// Shows a hint when a MIDI controller is connected or removed while the
    /// app is running. Controllers are only set up at startup, so a newly
    /// connected one needs a restart of the app.
    public static synchronized void registerDeviceCallback(Context context) {
        MidiManager manager = midiManager(context);
        if (manager == null || sDeviceCallbackRegistered) {
            return;
        }
        sDeviceCallbackRegistered = true;
        for (MidiDeviceInfo info : devices(context)) {
            sKnownDeviceIds.add(info.getId());
        }
        final Context appContext = context.getApplicationContext();
        final Handler mainHandler = new Handler(Looper.getMainLooper());
        manager.registerDeviceCallback(new MidiManager.DeviceCallback() {
            @Override
            public void onDeviceAdded(MidiDeviceInfo info) {
                synchronized (MidiBridge.class) {
                    if (!sKnownDeviceIds.add(info.getId())) {
                        return;
                    }
                }
                String name = getDeviceName(appContext, info.getId());
                Log.i(TAG, "MIDI device connected: " + name);
                mainHandler.post(() -> Toast.makeText(appContext,
                        name + " connected. Restart Vynix DJ to use it.",
                        Toast.LENGTH_LONG).show());
            }

            @Override
            public void onDeviceRemoved(MidiDeviceInfo info) {
                synchronized (MidiBridge.class) {
                    sKnownDeviceIds.remove(info.getId());
                }
                Log.i(TAG, "MIDI device removed: " + info.getId());
                mainHandler.post(() -> Toast.makeText(appContext,
                        "DJ controller disconnected.",
                        Toast.LENGTH_LONG).show());
            }
        }, mainHandler);
    }

    /// Returns the ids of all MIDI devices currently known to the system.
    public static int[] getDeviceIds(Context context) {
        MidiDeviceInfo[] infos = devices(context);
        int[] ids = new int[infos.length];
        for (int i = 0; i < infos.length; ++i) {
            ids[i] = infos[i].getId();
        }
        return ids;
    }

    /// Returns a human readable name, preferring the product name reported
    /// by USB devices since that is what controller mappings are named after.
    public static String getDeviceName(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        if (info == null) {
            return "";
        }
        Bundle properties = info.getProperties();
        String name = properties.getString(MidiDeviceInfo.PROPERTY_PRODUCT);
        if (name == null || name.isEmpty()) {
            name = properties.getString(MidiDeviceInfo.PROPERTY_NAME);
        }
        String manufacturer = properties.getString(MidiDeviceInfo.PROPERTY_MANUFACTURER);
        if (name == null || name.isEmpty()) {
            name = manufacturer != null ? manufacturer : "MIDI device " + deviceId;
        } else if (manufacturer != null && !manufacturer.isEmpty()
                && !name.toLowerCase().contains(manufacturer.toLowerCase())) {
            name = manufacturer + " " + name;
        }
        return name;
    }

    public static String getManufacturer(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        if (info == null) {
            return "";
        }
        String manufacturer =
                info.getProperties().getString(MidiDeviceInfo.PROPERTY_MANUFACTURER);
        return manufacturer != null ? manufacturer : "";
    }

    public static String getSerialNumber(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        if (info == null) {
            return "";
        }
        String serial = info.getProperties().getString(MidiDeviceInfo.PROPERTY_SERIAL_NUMBER);
        return serial != null ? serial : "";
    }

    /// One of MidiDeviceInfo.TYPE_USB, TYPE_VIRTUAL or TYPE_BLUETOOTH, or -1.
    public static int getDeviceType(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        return info != null ? info.getType() : -1;
    }

    /// Number of ports the device sends data on (controller -> app).
    public static int getOutputPortCount(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        return info != null ? info.getOutputPortCount() : 0;
    }

    /// Number of ports the device receives data on (app -> controller).
    public static int getInputPortCount(Context context, int deviceId) {
        MidiDeviceInfo info = findDevice(context, deviceId);
        return info != null ? info.getInputPortCount() : 0;
    }

    /// Opens the first input and output port of a device. Blocks until the
    /// device is open, so it must not be called from the Android main thread.
    /// Returns null on failure.
    public static MidiBridge open(Context context, int deviceId, long nativeController) {
        MidiManager manager = midiManager(context);
        MidiDeviceInfo info = findDevice(context, deviceId);
        if (manager == null || info == null) {
            Log.w(TAG, "MIDI device " + deviceId + " not found");
            return null;
        }
        final MidiDevice[] opened = new MidiDevice[1];
        final CountDownLatch latch = new CountDownLatch(1);
        manager.openDevice(info, device -> {
            opened[0] = device;
            latch.countDown();
        }, handler());
        try {
            if (!latch.await(OPEN_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                Log.w(TAG, "Timeout opening MIDI device " + deviceId);
                return null;
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return null;
        }
        MidiDevice device = opened[0];
        if (device == null) {
            Log.w(TAG, "Failed to open MIDI device " + deviceId);
            return null;
        }
        // Some controllers expose several ports (e.g. one per deck section),
        // so listen to all of them.
        List<MidiOutputPort> outputPorts = new ArrayList<>();
        for (int i = 0; i < info.getOutputPortCount(); ++i) {
            MidiOutputPort port = device.openOutputPort(i);
            if (port != null) {
                outputPorts.add(port);
            } else {
                Log.w(TAG, "Failed to open output port " + i + " of MIDI device " + deviceId);
            }
        }
        MidiInputPort inputPort =
                info.getInputPortCount() > 0 ? device.openInputPort(0) : null;
        if (outputPorts.isEmpty() && inputPort == null) {
            Log.w(TAG, "No usable port on MIDI device " + deviceId);
            closeQuietly(device);
            return null;
        }
        Log.i(TAG, "Opened MIDI device " + deviceId + " with " + outputPorts.size()
                + " output port(s), input port: " + (inputPort != null));
        return new MidiBridge(device, outputPorts, inputPort, nativeController);
    }

    private MidiBridge(MidiDevice device,
            List<MidiOutputPort> outputPorts,
            MidiInputPort inputPort,
            long nativeController) {
        mDevice = device;
        mOutputPorts = outputPorts;
        mInputPort = inputPort;
        mReceiver = new Receiver(nativeController);
        for (MidiOutputPort port : mOutputPorts) {
            port.connect(mReceiver);
        }
    }

    public boolean hasInput() {
        return !mOutputPorts.isEmpty();
    }

    public boolean hasOutput() {
        return mInputPort != null;
    }

    /// Sends raw MIDI bytes to the device.
    public boolean send(byte[] data) {
        if (mInputPort == null) {
            return false;
        }
        try {
            mInputPort.send(data, 0, data.length);
            return true;
        } catch (IOException e) {
            Log.w(TAG, "Failed to send MIDI data", e);
            return false;
        }
    }

    /// Stops forwarding data to the native side before closing the ports, so
    /// the native controller can be destroyed safely afterwards.
    public void close() {
        mReceiver.detach();
        for (MidiOutputPort port : mOutputPorts) {
            port.disconnect(mReceiver);
            closeQuietly(port);
        }
        closeQuietly(mInputPort);
        closeQuietly(mDevice);
    }

    private static void closeQuietly(java.io.Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException e) {
            Log.w(TAG, "Failed to close MIDI resource", e);
        }
    }

    private static class Receiver extends MidiReceiver {
        private long mNativeController;

        Receiver(long nativeController) {
            mNativeController = nativeController;
        }

        synchronized void detach() {
            mNativeController = 0;
        }

        @Override
        public synchronized void onSend(byte[] data, int offset, int count, long timestamp) {
            if (mNativeController != 0 && count > 0) {
                nativeOnMidiData(mNativeController, data, offset, count, timestamp);
            }
        }
    }
}
