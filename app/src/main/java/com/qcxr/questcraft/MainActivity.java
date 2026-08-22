package com.qcxr.questcraft;

import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;

import com.microsoft.aad.msal4j.DeviceCode;
import com.qcxr.questcraft.ui.UIActivity;
import com.qcxr.questcraft.utils.Constants;

import java.lang.ref.WeakReference;
import java.util.Optional;

import androidx.activity.ComponentActivity;

import org.angelauramc.judgelib.JudgeLibAPI;
import org.angelauramc.judgelib.impl.InitInfo;
import org.angelauramc.judgelib.launcher.AndroidJavaLauncher;

public class MainActivity extends ComponentActivity {
    public static WeakReference<MainActivity> weakMe = new WeakReference<>(null);
    public static Optional<MainActivity> instance() {
        return Optional.ofNullable(weakMe.get());
    }

    private static Handler uiThreadHandler;
    public static JudgeLibAPI judgeLibAPI = JudgeLibAPI.getInstance();
    public static String userLoginCode = null;

    private View questLauncherView;
    private NativeSurface nativeSurface;
    private XRActivityInput xrActivityInput;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        weakMe = new WeakReference<>(this);
        uiThreadHandler = new Handler(Looper.getMainLooper());
        //xrActivityInput = new XRActivityInput(uiThreadHandler);

        // JudgeLib Init
        initJudgeLib();

        MojangSkinFetcher.fetchSkin("flamgop").whenComplete((result,$) -> {
            JniBridge.setSkinImage(result.skinPngBytes(), result.slim());
        });

        //JniBridge.start(this, getAssets());
        setContentView(UIActivity.createView(this));
    }

    private void initJudgeLib() {
        // Initialize JudgeLib with mandatory info
        judgeLibAPI.initialize(new InitInfo(
                Constants.CLIENT_ID,
                Constants.LOGIN_AUTHORITY,
                Constants.INTERNAL_DATA_PATH().toString(),
                this::deviceCodeCallback
        ));
    }

    @Override
    protected void onDestroy() {
        JniBridge.stop();
        super.onDestroy();
    }

    private void deviceCodeCallback(DeviceCode res) {
        if (res != null) {
            userLoginCode = res.userCode();
        }
    }

    public void setVulkanSurface(Surface surface, int width, int height) {
        runOnUiThread(() -> {
            this.nativeSurface = new NativeSurface(this);
            questLauncherView = UIActivity.createView(this);

            this.nativeSurface.setSurface(surface);

            this.nativeSurface.setChildView(questLauncherView);

            this.setContentView(this.nativeSurface, new ViewGroup.LayoutParams(width, height));
        });
    }

    public void performSystemExit() {
        uiThreadHandler.post(() -> {
            System.exit(0);
        });
    }

    public void requestUiRender() {
        if (this.nativeSurface != null) this.nativeSurface.requestUiRender();
    }

    public void processPointerEvent(int pointerId, int action, float normX, float normY) {
        if (this.questLauncherView == null) return;
        this.xrActivityInput.processPointerEvent(questLauncherView, pointerId, action, normX, normY);
    }
}