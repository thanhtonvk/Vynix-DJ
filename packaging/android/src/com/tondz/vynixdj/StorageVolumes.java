package com.tondz.vynixdj;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.UriPermission;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.provider.DocumentsContract;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/// Access to removable volumes (USB OTG drives, SD cards).
///
/// Listing /storage directly does not work on all devices: on some Android
/// versions it is not readable by apps, even with all files access. USB
/// drives are often not mounted below /storage at all, they are only
/// accessible through the Storage Access Framework after the user granted
/// access to them.
public class StorageVolumes {
    private static final String TAG = "VynixStorage";
    private static final String EXTERNAL_STORAGE_AUTHORITY =
            "com.android.externalstorage.documents";
    static final int REQUEST_VOLUME_ACCESS = 4711;

    /// Volume can be read through its file system path
    public static final String ACCESS_DIRECT = "direct";
    /// Volume can be read through the Storage Access Framework
    public static final String ACCESS_GRANTED = "granted";
    /// The user needs to grant access first, see requestAccess()
    public static final String ACCESS_NONE = "none";

    private static native void nativeOnAccessGranted();

    /// Returns one entry per mounted removable volume, formatted as
    /// "<uuid>\t<path>\t<description>\t<access>".
    public static String[] getRemovableVolumes(Context context) {
        List<String> result = new ArrayList<>();
        for (StorageVolume volume : removableVolumes(context)) {
            String uuid = volume.getUuid();
            File directory = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                directory = volume.getDirectory();
            } else if (uuid != null) {
                directory = new File("/storage", uuid);
            }
            String access;
            if (directory != null && directory.canRead()) {
                access = ACCESS_DIRECT;
            } else if (uuid != null && findTree(context, uuid) != null) {
                access = ACCESS_GRANTED;
            } else if (uuid != null) {
                access = ACCESS_NONE;
            } else {
                continue;
            }
            String description = volume.getDescription(context);
            result.add((uuid != null ? uuid : "") + "\t"
                    + (directory != null ? directory.getAbsolutePath() : "") + "\t"
                    + (description != null ? description : uuid) + "\t"
                    + access);
        }
        return result.toArray(new String[0]);
    }

    /// Opens the system picker to grant access to a volume. The volume root
    /// must be selected, the result is handled in onActivityResult().
    public static void requestAccess(Context context, String uuid) {
        if (!(context instanceof Activity)) {
            Log.w(TAG, "Cannot request volume access without an activity");
            return;
        }
        final Activity activity = (Activity) context;
        activity.runOnUiThread(() -> {
            Intent intent = null;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                for (StorageVolume volume : removableVolumes(activity)) {
                    if (uuid.equals(volume.getUuid())) {
                        intent = volume.createOpenDocumentTreeIntent();
                        break;
                    }
                }
            }
            if (intent == null) {
                intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,
                            DocumentsContract.buildRootUri(
                                    EXTERNAL_STORAGE_AUTHORITY, uuid));
                }
            }
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            try {
                activity.startActivityForResult(intent, REQUEST_VOLUME_ACCESS);
            } catch (Exception e) {
                Log.w(TAG, "Failed to open the folder picker", e);
            }
        });
    }

    /// Persists the access granted in the picker. Returns false if the
    /// result does not belong to requestAccess().
    static boolean onActivityResult(Activity activity, int requestCode, int resultCode,
            Intent data) {
        if (requestCode != REQUEST_VOLUME_ACCESS) {
            return false;
        }
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            return true;
        }
        Uri tree = data.getData();
        try {
            activity.getContentResolver().takePersistableUriPermission(
                    tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException e) {
            Log.w(TAG, "Failed to persist access to " + tree, e);
            return true;
        }
        Log.i(TAG, "Access granted to " + tree);
        try {
            nativeOnAccessGranted();
        } catch (UnsatisfiedLinkError e) {
            // The library feature is not initialized yet
        }
        return true;
    }

    /// Copies a file of a volume with granted access to destPath. The copy is
    /// skipped if destPath already exists with the same size, unless force is
    /// set. relativePath uses '/' as separator, e.g. "PIONEER/rekordbox/export.pdb".
    public static boolean copyFile(Context context, String uuid, String relativePath,
            String destPath, boolean force) {
        Uri tree = findTree(context, uuid);
        if (tree == null) {
            return false;
        }
        String treeDocumentId = DocumentsContract.getTreeDocumentId(tree);
        String path = relativePath.startsWith("/") ? relativePath.substring(1) : relativePath;
        String documentId = treeDocumentId.endsWith(":")
                ? treeDocumentId + path
                : treeDocumentId + "/" + path;
        Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, documentId);
        ContentResolver resolver = context.getContentResolver();
        File dest = new File(destPath);
        long size = documentSize(resolver, document);
        if (size < 0) {
            return false;
        }
        if (!force && dest.isFile() && dest.length() == size) {
            // Mark as recently used for trimCache()
            dest.setLastModified(System.currentTimeMillis());
            return true;
        }
        File parent = dest.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }
        File temp = new File(destPath + ".part");
        try (InputStream in = resolver.openInputStream(document);
                OutputStream out = new FileOutputStream(temp)) {
            if (in == null) {
                return false;
            }
            byte[] buffer = new byte[1 << 16];
            int count;
            while ((count = in.read(buffer)) > 0) {
                out.write(buffer, 0, count);
            }
        } catch (IOException | SecurityException | IllegalArgumentException e) {
            Log.w(TAG, "Failed to copy " + document, e);
            temp.delete();
            return false;
        }
        if (!temp.renameTo(dest)) {
            temp.delete();
            return false;
        }
        return true;
    }

    /// Deletes the least recently used files below directory until it uses
    /// at most maxBytes. keepPath is never deleted.
    public static void trimCache(String directory, long maxBytes, String keepPath) {
        List<File> files = new ArrayList<>();
        collectFiles(new File(directory), files);
        long total = 0;
        for (File file : files) {
            total += file.length();
        }
        if (total <= maxBytes) {
            return;
        }
        File[] sorted = files.toArray(new File[0]);
        Arrays.sort(sorted, (a, b) -> Long.compare(a.lastModified(), b.lastModified()));
        for (File file : sorted) {
            if (total <= maxBytes) {
                break;
            }
            if (file.getAbsolutePath().equals(keepPath) || file.getName().endsWith(".pdb")) {
                continue;
            }
            long length = file.length();
            if (file.delete()) {
                total -= length;
            }
        }
    }

    private static void collectFiles(File directory, List<File> files) {
        File[] children = directory.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            if (child.isDirectory()) {
                collectFiles(child, files);
            } else {
                files.add(child);
            }
        }
    }

    private static long documentSize(ContentResolver resolver, Uri document) {
        try (Cursor cursor = resolver.query(document,
                     new String[] {DocumentsContract.Document.COLUMN_SIZE},
                     null,
                     null,
                     null)) {
            if (cursor != null && cursor.moveToFirst()) {
                return cursor.isNull(0) ? 0 : cursor.getLong(0);
            }
        } catch (SecurityException | IllegalArgumentException e) {
            // Missing file or no access
        }
        return -1;
    }

    /// Returns the persisted tree URI for the root of the volume, or null.
    private static Uri findTree(Context context, String uuid) {
        for (UriPermission permission :
                context.getContentResolver().getPersistedUriPermissions()) {
            Uri uri = permission.getUri();
            if (!permission.isReadPermission()
                    || !EXTERNAL_STORAGE_AUTHORITY.equals(uri.getAuthority())) {
                continue;
            }
            try {
                if ((uuid + ":").equals(DocumentsContract.getTreeDocumentId(uri))) {
                    return uri;
                }
            } catch (IllegalArgumentException e) {
                // Not a tree URI
            }
        }
        return null;
    }

    private static List<StorageVolume> removableVolumes(Context context) {
        List<StorageVolume> volumes = new ArrayList<>();
        StorageManager manager = context != null
                ? (StorageManager) context.getSystemService(Context.STORAGE_SERVICE)
                : null;
        if (manager == null) {
            return volumes;
        }
        for (StorageVolume volume : manager.getStorageVolumes()) {
            if (volume.isRemovable()
                    && Environment.MEDIA_MOUNTED.equals(volume.getState())) {
                volumes.add(volume);
            }
        }
        return volumes;
    }
}
