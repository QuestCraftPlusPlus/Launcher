package com.qcxr.questcraft;

import android.annotation.SuppressLint;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import com.qcxr.questcraft.ui.UIActivity;

import java.lang.ref.WeakReference;

import androidx.activity.ComponentActivity;

public class MainActivity extends ComponentActivity {
    private static Handler uiThreadHandler;
    public static WeakReference<MainActivity> weakMe;

    public static View questLauncherView;
    private NativeSurface nativeSurface;

    static {
        System.loadLibrary("qcxr");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setDensity();
        uiThreadHandler = new Handler(Looper.getMainLooper());
        weakMe = new WeakReference<>(this);
        start(new XRActivityInput(), getAssets());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stop();
    }

    private void setDensity() {
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        float density = 0.75f;
        float densityMultiplier = 160;
        metrics.density = density;
        metrics.ydpi = metrics.xdpi = densityMultiplier * density;
        metrics.densityDpi = (int)metrics.xdpi;
        getResources().updateConfiguration(null, null);
    }

    private native void start(XRActivityInput xrActivityInput, AssetManager assetManager);
    private native void stop();

    @SuppressLint("SetJavaScriptEnabled")
    public static void setVulkanSurface(Surface surface) {
        MainActivity me = weakMe.get();
        if (me == null) return;

        int w = 2560;
        int h = 1440;

        me.runOnUiThread(() -> {
            me.nativeSurface = new NativeSurface(me);
            questLauncherView = UIActivity.createView(me);

            me.nativeSurface.setSurface(surface);

            me.nativeSurface.setChildView(questLauncherView);

            me.setContentView(me.nativeSurface, new ViewGroup.LayoutParams(w, h));
        });
    }

    public static void performSystemExit() {
        uiThreadHandler.post(()->{
            weakMe.get();
            System.exit(0);
        });
    }
}