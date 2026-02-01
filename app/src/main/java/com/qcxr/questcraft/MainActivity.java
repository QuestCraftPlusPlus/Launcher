package com.qcxr.questcraft;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.res.AssetManager;
import android.graphics.SurfaceTexture;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ViewGroup;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import java.lang.ref.WeakReference;

public class MainActivity extends Activity {
    private static Handler uiThreadHandler;
    private static WeakReference<MainActivity> weakMe;
    private NativeSurface nativeSurface;
    @SuppressLint("StaticFieldLeak")
    public static WebView webView;

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

    public static void updateSurfaceTexture() {
        MainActivity me = weakMe.get();
        if(me == null || me.nativeSurface == null) return;
        me.nativeSurface.updateSurfaceTexture();
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void createNativeView(SurfaceTexture surfaceTexture, int w, int h) {
        nativeSurface = new NativeSurface(this);
        nativeSurface.setSurfaceTexture(surfaceTexture);

        webView = new WebView(this);
        nativeSurface.setChildView(webView);
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return false;
            }
        });
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        webView.loadUrl("https://youtu.be/PomiV1iyTp8?t=54");

        setContentView(nativeSurface, new ViewGroup.LayoutParams(w, h));
    }

    public static boolean createSurfaceTexture(int texId) {
        int w = 2560, h = 1440;
        try {
            SurfaceTexture surfaceTexture = new SurfaceTexture(texId);
            surfaceTexture.setDefaultBufferSize(w, h);
            MainActivity me = weakMe.get();
            if(me == null) throw new NullPointerException("MainActivity reference missing");
            me.runOnUiThread(()->me.createNativeView(surfaceTexture, w, h));
            return true;
        }catch (Exception e) {
            Log.e("MainActivity", "Failed to create surface texture", e);
            return false;
        }
    }

    public static void performSystemExit() {
        uiThreadHandler.post(()->{
            MainActivity me = weakMe.get();
            if(me != null && me.nativeSurface != null) {
                me.nativeSurface.destroySurfaceTexture();
            }
            System.exit(0);
        });
    }
}