package com.qcxr.questcraft;

import android.os.Handler;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.view.View;

import java.util.HashMap;
import java.util.Map;

public class XRActivityInput {
    private static class PointerState {
        long downTime;
        boolean isDown = false;
    }

    private final Map<Integer, PointerState> pointerStates = new HashMap<>();
    private final Handler uiHandler;

    public XRActivityInput(Handler uiHandler) {
        this.uiHandler = uiHandler;
    }

    public void processPointerEvent(View view, int pointerId, int action, float normX, float normY) {
        if (view == null || uiHandler == null) return;

        uiHandler.post(() -> {
            int viewWidth = view.getWidth();
            int viewHeight = view.getHeight();

            float absX = normX * viewWidth;
            float absY = normY * viewHeight;

            long currentTime = SystemClock.uptimeMillis();

            PointerState state = pointerStates.computeIfAbsent(pointerId, k -> new PointerState());

            switch (action) {
                case MotionEvent.ACTION_DOWN -> {
                    state.downTime = currentTime;
                    state.isDown = true;
                }
                case MotionEvent.ACTION_UP -> {
                    state.isDown = false;
                }
                case MotionEvent.ACTION_MOVE -> {
                    if (!state.isDown) {
                        state.downTime = currentTime;
                        state.isDown = true;
                    }
                }
            }

            MotionEvent event = MotionEvent.obtain(
                    state.downTime,
                    currentTime,
                    action,
                    absX,
                    absY,
                    0
            );

            view.dispatchTouchEvent(event);
            event.recycle();
        });
    }
}
