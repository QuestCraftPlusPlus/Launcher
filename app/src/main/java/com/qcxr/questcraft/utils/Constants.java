package com.qcxr.questcraft.utils;

import android.annotation.SuppressLint;

import com.qcxr.questcraft.MainActivity;

import java.io.File;
import java.nio.file.Path;

public final class Constants {

    public static final String CLIENT_ID = "d17a73a2-707c-40f5-8c90-d3eda0956f10";
    public static final String LOGIN_AUTHORITY = "https://login.microsoftonline.com/consumers/";

    @SuppressLint("SdCardPath")
    public static Path INTERNAL_DATA_PATH() {
        MainActivity me = MainActivity.weakMe.get();
        if (me == null) {
            return new File("/data/data/com.qcxr.questcraft/files").toPath();
        }

        return me.getFilesDir().toPath();
    }

    @SuppressLint("SdCardPath")
    public static Path USER_DATA_PATH() {
        MainActivity me = MainActivity.weakMe.get();
        if (me == null) {
            return new File("/sdcard/Android/data/com.qcxr.questcraft/files").toPath();
        }

        return me.getExternalFilesDir(null).toPath();
    }

    public static Path INSTANCE_ROOT_PATH() {
        return USER_DATA_PATH().resolve("instances");
    }

    public static Path MINECRAFT_ASSETS_PATH() {
        return INTERNAL_DATA_PATH().resolve("assets");
    }

    public static Path MINECRAFT_LIBRARIES_PATH() {
        return INTERNAL_DATA_PATH().resolve("libraries");
    }

    public static Path JAVA_RUNTIMES_PATH() {
        return INTERNAL_DATA_PATH().resolve("runtimes");
    }
}
