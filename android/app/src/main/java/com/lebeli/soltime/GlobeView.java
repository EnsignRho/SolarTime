//////////
//
// GlobeView.java -- GLSurfaceView wrapper:  one-finger drag spins/tilts the globe,
// pinch zooms.  Drag sensitivity scales with zoom so close-in navigation stays gentle.
//
//////
package com.lebeli.soltime;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;

public class GlobeView extends GLSurfaceView
{
	public final GlobeRenderer			renderer;
	private final ScaleGestureDetector	pinch;
	private float						lastX, lastY;
	private boolean						dragging = false;

	public GlobeView(Context context)
	{
		super(context);
		setEGLContextClientVersion(2);
		renderer = new GlobeRenderer(context);
		setRenderer(renderer);
		setRenderMode(RENDERMODE_CONTINUOUSLY);					// the terminator and clock are alive

		pinch = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener()
		{
			@Override
			public boolean onScale(ScaleGestureDetector d)
			{
				float dist = renderer.dist / d.getScaleFactor();
				renderer.dist = Math.max(1.6f, Math.min(8.0f, dist));
				return true;
			}
		});
	}

	@Override
	public boolean onTouchEvent(MotionEvent e)
	{
		pinch.onTouchEvent(e);

		switch (e.getActionMasked())
		{
			case MotionEvent.ACTION_DOWN:
				lastX = e.getX();  lastY = e.getY();
				dragging = true;
				break;

			case MotionEvent.ACTION_POINTER_DOWN:				// second finger = pinch, stop dragging
				dragging = false;
				break;

			case MotionEvent.ACTION_MOVE:
				if (dragging && e.getPointerCount() == 1)
				{
					float scale = 0.12f * (renderer.dist - 1.0f);	// gentler when zoomed in
					renderer.yawDeg   += (e.getX() - lastX) * scale;
					float pitch        = renderer.pitchDeg + (e.getY() - lastY) * scale;
					renderer.pitchDeg  = Math.max(-85.0f, Math.min(85.0f, pitch));
					lastX = e.getX();  lastY = e.getY();
				}
				break;

			case MotionEvent.ACTION_UP:
			case MotionEvent.ACTION_CANCEL:
				dragging = false;
				break;
		}
		return true;
	}
}
