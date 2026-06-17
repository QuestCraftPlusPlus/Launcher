package com.qcxr.questcraft.utils;

import com.qcxr.questcraft.MainActivity;

import java.io.File;
import java.nio.file.Path;

public final class Constants {

    public static Path ROOT_DATA_PATH() {
        return MainActivity.weakMe.get().getFilesDir().toPath();
    }

    public static Path INSTANCE_ROOT_PATH() {
        Path filesDir = MainActivity.weakMe.get().getFilesDir().toPath();
        return filesDir.resolve("instances");
    }
}
