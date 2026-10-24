package com.tondz.vynixdj;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import org.qtproject.qt.android.QtActivityBase;

public class MainActivity extends QtActivityBase {
    static {
        // Must be loaded before the Qt libraries: it provides libc functions
        // the prebuilt libraries need, which older Android versions lack.
        // See src/util/androidlibccompat.c
        try {
            System.loadLibrary("vynixcompat");
        } catch (UnsatisfiedLinkError e) {
            Log.e("VynixDJ", "Failed to load libvynixcompat.so", e);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        // Draw into the camera cutout area: in landscape the cutout is a small
        // hole at the short edge, keeping it free would waste a full strip.
        WindowManager.LayoutParams lp = this.getWindow().getAttributes();
        lp.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(lp);
        // Edge to edge, so the Qt view is not inset by the cutout area
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);

        super.onCreate(savedInstanceState);


        hideSystemBars();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (StorageVolumes.onActivityResult(this, requestCode, resultCode, data)) {
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    @Override
    public void onResume() {
        super.onResume();
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        // Qt shows the system bars again when it sets up its window, so hide
        // them whenever the app (re)gains focus.
        if (hasFocus) {
            hideSystemBars();
        }
    }

    /// Hides the status and navigation bars to use the whole screen and to
    /// prevent accidental back or app switch gestures. A swipe from the edge
    /// shows them transiently.
    private void hideSystemBars() {
        WindowInsetsControllerCompat windowInsetsController =
            WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
        windowInsetsController.setSystemBarsBehavior(
            WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        windowInsetsController.hide(WindowInsetsCompat.Type.systemBars());
    }
}
