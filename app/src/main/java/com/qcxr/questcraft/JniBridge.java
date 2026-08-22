package com.qcxr.questcraft;

import android.content.res.AssetManager;
import android.view.Surface;
import com.qcxr.questcraft.ui.XrTextInputBridge;

@SuppressWarnings("unused")
public class JniBridge {
    static {
        System.loadLibrary("qcxr");
    }

    public static native void setSkinImage(byte[] imageBytes, boolean slim);
    public static native void start(MainActivity activity, AssetManager assetManager);
    public static native void stop();
    public static native void showKeyboard();
    public static native void hideKeyboard();

    public static void setVulkanSurface(Surface surface, int width, int height) {
        MainActivity.instance().ifPresent(me -> me.setVulkanSurface(surface, width, height));
    }

    public static void performSystemExit() {
        MainActivity.instance().ifPresent(MainActivity::performSystemExit);
    }

    public static void requestUiRender() {
        MainActivity.instance().ifPresent(MainActivity::requestUiRender);
    }

    public static void processPointerEvent(int pointerId, int action, float normX, float normY) {
        MainActivity.instance().ifPresent(me -> me.processPointerEvent(pointerId, action, normX, normY));
    }

    public static void sendText(String text) {
        XrTextInputBridge.sendText(text);
    }

    public static void deleteCharacter() {
        XrTextInputBridge.deleteCharacter();
    }
}
