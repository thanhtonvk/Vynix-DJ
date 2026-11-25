package com.tondz.vynixdj;

import android.content.Context;
import android.media.MediaScannerConnection;
import android.os.Handler;
import android.os.Looper;
import android.widget.Toast;
import java.io.File;

/// Called by native code when a recording has been saved.
public class Recordings {
    /// Shows where the recording is and adds the files to the media library of
    /// the device, so that music players and the share dialog list them.
    /// paths: absolute paths, separated by newlines
    public static void onSaved(Context context, String paths) {
        final Context appContext = context.getApplicationContext();
        final String[] files = paths.split("\n");
        MediaScannerConnection.scanFile(appContext, files, null, null);
        final File file = new File(files[0]);
        final String folder = file.getParentFile().getPath()
                .replaceFirst("^/storage/emulated/0/", "");
        new Handler(Looper.getMainLooper()).post(() -> Toast.makeText(appContext,
                "Recording saved: " + folder + "/" + file.getName(),
                Toast.LENGTH_LONG).show());
    }
}
