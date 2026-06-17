package com.qcxr.questcraft.utils;

import com.qcxr.questcraft.MainActivity;

import java.io.File;
import java.nio.file.Path;

public final class Constants {

    public static Path ROOT_DATA_PATH() {
        return MainActivity.weakMe.get().getFilesDir().toPath();
    }

    public static Path INSTANCE_ROOT_PATH() {
        return ROOT_DATA_PATH().resolve("instances");
    }

    public static Path MINECRAFT_ASSETS_PATH() {
        return ROOT_DATA_PATH().resolve("assets");
    }

    public static Path MINECRAFT_LIBRARIES_PATH() {
        return ROOT_DATA_PATH().resolve("libraries");
    }
}
