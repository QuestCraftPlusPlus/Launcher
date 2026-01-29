package com.qcxr.questcraft;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.SurfaceTexture;
import android.view.Choreographer;
import android.view.Surface;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

public class NativeSurface extends FrameLayout implements Choreographer.FrameCallback {
    private boolean hasSurface = false;
    private SurfaceTexture surfaceTexture;
    private Surface surface;
    private final Choreographer choreographer;

    public NativeSurface(Context context) {
        super(context);
        choreographer = Choreographer.getInstance();
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        // Discard any draw requests initiated by Android itself. We draw into our own,
        // personal surface.
    }

    public void setSurfaceTexture(SurfaceTexture surfaceTexture) {
        post(()->setSurfaceTextureInternal(surfaceTexture));
    }

    private void setSurfaceTextureInternal(SurfaceTexture surfaceTexture) {
        this.surfaceTexture = surfaceTexture;
        surface = new Surface(surfaceTexture);
        hasSurface = true;
        choreographer.postFrameCallback(this);
    }

    public void updateSurfaceTexture() {
        if(!hasSurface) return;
        surfaceTexture.updateTexImage();
    }

    public void destroySurfaceTexture() {
        hasSurface = false;
        onDetachedFromWindow();
        if(surfaceTexture != null) surfaceTexture.release();
    }

    public void setChildView(View view) {
        FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );
        addView(view, layoutParams);
    }

    @Override
    public void doFrame(long l) {
        if(!hasSurface) return;
        Canvas canvas = surface.lockHardwareCanvas();
        if(getChildCount() > 0) getChildAt(0).draw(canvas);
        surface.unlockCanvasAndPost(canvas);
        choreographer.postFrameCallback(this);
    }
}
