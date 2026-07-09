package com.qcxr.questcraft;

import android.os.SystemClock;
import android.view.MotionEvent;

import java.util.HashMap;
import java.util.Map;

public class XRActivityInput {

    private static class PointerState {
        long downTime;
        boolean isDown = false;
    }

    private static final Map<Integer, PointerState> pointerStates = new HashMap<>();

    public static void processPointerEvent(int pointerId, int action, float normX, float normY) {
        if (MainActivity.weakMe == null) return;
        MainActivity me = MainActivity.weakMe.get();
        if (me == null || MainActivity.questLauncherView == null) return;

        me.runOnUiThread(() -> {
            int viewWidth = MainActivity.questLauncherView.getWidth();
            int viewHeight = MainActivity.questLauncherView.getHeight();

            float absX = normX * viewWidth;
            float absY = normY * viewHeight;

            long currentTime = SystemClock.uptimeMillis();

            PointerState state = pointerStates.computeIfAbsent(pointerId, k -> new PointerState());

            int androidAction = -1;
            switch (action) {
                case MotionEvent.ACTION_DOWN -> {
                    state.downTime = currentTime;
                    state.isDown = true;
                    androidAction = MotionEvent.ACTION_DOWN;
                }
                case MotionEvent.ACTION_UP -> {
                    if (!state.isDown) return;
                    androidAction = MotionEvent.ACTION_UP;
                    state.isDown = false;
                }
                case MotionEvent.ACTION_MOVE -> {
                    if (!state.isDown) {
                        state.downTime = currentTime;
                        state.isDown = true;
                        androidAction = MotionEvent.ACTION_DOWN;
                    } else {
                        androidAction = MotionEvent.ACTION_MOVE;
                    }
                }
            }

            if (androidAction != -1) {
                MotionEvent event = MotionEvent.obtain(
                        state.downTime,
                        currentTime,
                        androidAction,
                        absX,
                        absY,
                        0
                );

                MainActivity.questLauncherView.dispatchTouchEvent(event);
                event.recycle();
            }
        });
    }
}
