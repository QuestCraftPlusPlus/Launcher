package com.qcxr.questcraft;

import android.os.SystemClock;
import android.view.MotionEvent;

public class XRActivityInput {
    static void clickScreenAtPosition(float x, float y) {
        System.out.println("Clicking screen at position: " + x + ", " + y);

        if (MainActivity.weakMe != null) {
            MainActivity me = MainActivity.weakMe.get();
            if (me != null) {
                me.runOnUiThread(() -> { // TODO: we have triggers, we can probably just send in real motion events rather than trying to simulate a tap
                    long downTime = SystemClock.uptimeMillis();
                    long eventTime = SystemClock.uptimeMillis();

                    float absX = x * MainActivity.questLauncherView.getWidth();
                    float absY = y * MainActivity.questLauncherView.getHeight();

                    MotionEvent downEvent = MotionEvent.obtain(
                            downTime, eventTime, MotionEvent.ACTION_DOWN, absX, absY, 0
                    );
                    MotionEvent upEvent = MotionEvent.obtain(
                            downTime, eventTime + 50, MotionEvent.ACTION_UP, absX, absY, 0
                    );

                    MainActivity.questLauncherView.dispatchTouchEvent(downEvent);
                    MainActivity.questLauncherView.dispatchTouchEvent(upEvent);

                    downEvent.recycle();
                    upEvent.recycle();
                });
            }
        }
    }
}
