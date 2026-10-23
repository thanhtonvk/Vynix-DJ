package com.tondz.vynixdj;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.util.TypedValue;
import android.view.Gravity;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/// Launcher activity that asks for storage access before the Qt app starts.
///
/// Requesting the permission from native code while Mixxx initializes leaves
/// the app starting up in the background while the system settings screen is
/// shown, and Android may stop or reconfigure it meanwhile. Asking here first
/// means MainActivity always starts in the foreground with the final
/// permission state.
public class StoragePermissionActivity extends Activity {
    private static final String TAG = "VynixPermission";
    private static final String KEY_WAITING = "waitingForSettings";
    private static final int REQUEST_SETTINGS = 1;
    private static final int REQUEST_LEGACY = 2;

    private boolean mWaitingForSettings = false;
    private boolean mStarted = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            mWaitingForSettings = savedInstanceState.getBoolean(KEY_WAITING, false);
        }
        if (hasStorageAccess()) {
            startMainActivity();
            return;
        }
        if (!mWaitingForSettings) {
            showExplanation();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Some devices do not deliver a result for the settings screen, so
        // also continue when the user comes back from it.
        if (mWaitingForSettings) {
            startMainActivity();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putBoolean(KEY_WAITING, mWaitingForSettings);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_SETTINGS) {
            startMainActivity();
        }
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LEGACY) {
            startMainActivity();
        }
    }

    private boolean hasStorageAccess() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            return Environment.isExternalStorageManager();
        }
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED
                && checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)
                == PackageManager.PERMISSION_GRANTED;
    }

    private void requestStorageAccess() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            requestPermissions(new String[] {
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    REQUEST_LEGACY);
            return;
        }
        Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                Uri.parse("package:" + getPackageName()));
        try {
            mWaitingForSettings = true;
            startActivityForResult(intent, REQUEST_SETTINGS);
        } catch (ActivityNotFoundException e) {
            // Some vendors only provide the generic list of apps
            try {
                startActivityForResult(
                        new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION),
                        REQUEST_SETTINGS);
            } catch (ActivityNotFoundException e2) {
                Log.w(TAG, "No settings screen for all files access", e2);
                mWaitingForSettings = false;
                startMainActivity();
            }
        }
    }

    private void startMainActivity() {
        if (mStarted) {
            return;
        }
        mStarted = true;
        mWaitingForSettings = false;
        Log.i(TAG, "Storage access: " + hasStorageAccess());
        Intent intent = new Intent(this, MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivity(intent);
        finish();
        overridePendingTransition(0, 0);
    }

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    private void showExplanation() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackgroundColor(Color.rgb(0, 0, 20));
        layout.setPadding(dp(32), dp(24), dp(32), dp(24));

        ImageView logo = new ImageView(this);
        logo.setImageResource(getResources().getIdentifier(
                "splash_logo", "drawable", getPackageName()));
        logo.setAdjustViewBounds(true);
        layout.addView(logo, new LinearLayout.LayoutParams(dp(220), dp(110)));

        TextView text = new TextView(this);
        text.setText("Vynix DJ needs access to your files to find the music on "
                + "this device and on USB drives.");
        text.setTextColor(Color.WHITE);
        text.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        text.setGravity(Gravity.CENTER);
        text.setPadding(0, dp(16), 0, dp(24));
        layout.addView(text, new LinearLayout.LayoutParams(dp(480),
                LinearLayout.LayoutParams.WRAP_CONTENT));

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);
        buttons.setGravity(Gravity.CENTER);

        Button skip = new Button(this);
        skip.setText("Not now");
        skip.setOnClickListener(v -> startMainActivity());
        buttons.addView(skip);

        Button allow = new Button(this);
        allow.setText("Allow access");
        allow.setOnClickListener(v -> requestStorageAccess());
        LinearLayout.LayoutParams allowParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        allowParams.leftMargin = dp(16);
        buttons.addView(allow, allowParams);

        layout.addView(buttons);
        setContentView(layout);
    }
}
