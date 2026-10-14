//////////
//
// GlobeRenderer.java -- OpenGL ES 2.0 Earth for SolTime.
//
// A textured UV sphere (equirectangular Blue Marble from assets/earth.jpg, procedural
// fallback if the asset is missing), lit by the REAL sun:  the day/night terminator is
// computed from SolTime's solar position every frame, so the dark side of the globe is
// the actual dark side right now.  City dots ride at radius 1.005 (depth test hides the
// far side).  A fixed reticle at screen center picks the surface point facing the
// camera; MainActivity reads centerLat/centerLon and shows that place's SolTime.
//
// Rotation model:  M = Rx(pitch) * Ry(yaw), camera fixed at +Z.  The reticle pick is
// just M^-1 * (0,0,1) back to lat/lon -- no ray casting needed for a centered reticle.
//
//////
package com.lebeli.soltime;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.opengl.Matrix;

import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class GlobeRenderer implements GLSurfaceView.Renderer
{
	private static final int	STACKS			= 48;
	private static final int	SLICES			= 96;
	private static final float	CITY_RADIUS		= 1.005f;		// dots float just off the surface

	private final Context	context;

	// Camera / interaction state (written by GlobeView on the UI thread)
	public volatile float	yawDeg		= 0.0f;					// spins the globe (drag X)
	public volatile float	pitchDeg	= 0.0f;					// tilts it (drag Y), clamped +/-85
	public volatile float	dist		= 3.0f;					// camera distance (pinch), [1.6, 8]

	// Reticle pick (written on the GL thread, read by MainActivity's clock tick)
	public volatile double	centerLat	= 0.0;
	public volatile double	centerLon	= 0.0;

	// User GPS fix (written by MainActivity, NaN until the first fix)
	public volatile double	userLat		= Double.NaN;
	public volatile double	userLon		= Double.NaN;

	// Sphere geometry
	private FloatBuffer	sphereVerts;							// interleaved x,y,z,u,v
	private ShortBuffer	sphereIndex;
	private int			indexCount;

	// City dots (the zones led by cities of 1M+, set once the data loads) and the you-are-here marker
	private volatile FloatBuffer	dotVerts	= null;
	private volatile int			dotCount	= 0;
	private FloatBuffer	userVert	= null;						// rebuilt when the fix moves
	private double		userVertLat	= Double.NaN, userVertLon = Double.NaN;

	// Zone rings (spec §8):  each zone's lock, and the outer edge of its ramp -- one GL_LINES
	// buffer each, published together once the registry is built
	private volatile FloatBuffer	lockVerts	= null, rampVerts = null;
	private volatile int			lockVertCount = 0, rampVertCount = 0;

	// GL handles
	private int	earthProg, pointProg, lineProg, texEarth;
	private int	eaPos, eaUv, euMvp, euSun, euTex;
	private int	paPos, puMvp, puColor, puSize;
	private int	laPos, luMvp, luColor;

	// Matrices (GL thread only)
	private final float[]	proj	= new float[16];
	private final float[]	view	= new float[16];
	private final float[]	model	= new float[16];
	private final float[]	tmpA	= new float[16];
	private final float[]	tmpB	= new float[16];
	private final float[]	mvp		= new float[16];
	private final float[]	inv		= new float[16];
	private final float[]	vecIn	= new float[4];
	private final float[]	vec		= new float[4];
	private final float[]	sun		= new float[3];
	private final double[]	sunLL	= new double[2];

	public GlobeRenderer(Context context)		{ this.context = context; }


	//////////
	// Shaders.  Lighting runs in OBJECT (earth-fixed) space:  a unit sphere's normal IS
	// its position, and the sun direction arrives already earth-fixed from subsolar() --
	// no normal matrix, no transforms.
	//////
		private static final String EARTH_VS =
			  "uniform mat4 uMvp;\n"
			+ "attribute vec3 aPos;\n"
			+ "attribute vec2 aUv;\n"
			+ "varying vec3 vNormal;\n"
			+ "varying vec2 vUv;\n"
			+ "void main() {\n"
			+ "  vNormal = aPos;\n"
			+ "  vUv = aUv;\n"
			+ "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
			+ "}\n";

		private static final String EARTH_FS =
			  "precision mediump float;\n"
			+ "uniform sampler2D uTex;\n"
			+ "uniform vec3 uSunDir;\n"
			+ "varying vec3 vNormal;\n"
			+ "varying vec2 vUv;\n"
			+ "void main() {\n"
			+ "  float d = dot(normalize(vNormal), uSunDir);\n"
			+ "  float light = 0.22 + 0.88 * smoothstep(-0.06, 0.12, d);\n"	// soft terminator
			+ "  vec3 c = texture2D(uTex, vUv).rgb * light;\n"
			+ "  gl_FragColor = vec4(c, 1.0);\n"
			+ "}\n";

		private static final String POINT_VS =
			  "uniform mat4 uMvp;\n"
			+ "uniform float uSize;\n"
			+ "attribute vec3 aPos;\n"
			+ "void main() {\n"
			+ "  gl_Position = uMvp * vec4(aPos, 1.0);\n"
			+ "  gl_PointSize = uSize;\n"
			+ "}\n";

		private static final String LINE_FS =
			  "precision mediump float;\n"
			+ "uniform vec4 uColor;\n"
			+ "void main() { gl_FragColor = uColor; }\n";

		private static final String LINE_VS =
			  "uniform mat4 uMvp;\n"
			+ "attribute vec3 aPos;\n"
			+ "void main() { gl_Position = uMvp * vec4(aPos, 1.0); }\n";

		private static final String POINT_FS =
			  "precision mediump float;\n"
			+ "uniform vec4 uColor;\n"
			+ "void main() {\n"
			+ "  vec2 d = gl_PointCoord - vec2(0.5);\n"			// round dots, not squares
			+ "  if (dot(d, d) > 0.25) discard;\n"
			+ "  gl_FragColor = uColor;\n"
			+ "}\n";


	//////////
	// Lat/lon -> unit sphere.  Lon 0 faces +Z, east is +X at Greenwich, north is +Y.
	//////
		private static void llToXyz(double latDeg, double lonDeg, float radius, float[] out, int at)
		{
			double lat = Math.toRadians(latDeg), lon = Math.toRadians(lonDeg);
			out[at    ] = (float)(radius * Math.cos(lat) * Math.sin(lon));
			out[at + 1] = (float)(radius * Math.sin(lat));
			out[at + 2] = (float)(radius * Math.cos(lat) * Math.cos(lon));
		}


	//////////
	// Mesh + point buffers
	//////
		private void iBuildSphere()
		{
			int     vcount = (STACKS + 1) * (SLICES + 1);
			float[] v      = new float[vcount * 5];
			int     p      = 0;
			for (int st = 0; st <= STACKS; ++st)
			{
				double lat = 90.0 - 180.0 * st / (double)STACKS;
				for (int sl = 0; sl <= SLICES; ++sl)
				{
					double lon = -180.0 + 360.0 * sl / (double)SLICES;
					llToXyz(lat, lon, 1.0f, v, p);
					v[p + 3] = (float)(sl / (double)SLICES);				// u:  -180 -> 0, +180 -> 1
					v[p + 4] = (float)(st / (double)STACKS);				// v:  north pole 0 (bitmap top)
					p += 5;
				}
			}

			short[] ix = new short[STACKS * SLICES * 6];
			int     q  = 0;
			for (int st = 0; st < STACKS; ++st)
			{
				for (int sl = 0; sl < SLICES; ++sl)
				{
					short a = (short)( st      * (SLICES + 1) + sl);
					short b = (short)((st + 1) * (SLICES + 1) + sl);
					ix[q++] = a;  ix[q++] = b;                ix[q++] = (short)(a + 1);
					ix[q++] = b;  ix[q++] = (short)(b + 1);   ix[q++] = (short)(a + 1);
				}
			}

			sphereVerts = ByteBuffer.allocateDirect(v.length * 4).order(ByteOrder.nativeOrder()).asFloatBuffer();
			sphereVerts.put(v).position(0);
			sphereIndex = ByteBuffer.allocateDirect(ix.length * 2).order(ByteOrder.nativeOrder()).asShortBuffer();
			sphereIndex.put(ix).position(0);
			indexCount  = ix.length;
		}


	//////////
	// City dots:  called by MainActivity once the place data loads (any thread)
	//////
		public void setDots(double[] lat, double[] lon)
		{
			float[] cv = new float[lat.length * 3];
			for (int i = 0; i < lat.length; ++i)
				llToXyz(lat[i], lon[i], CITY_RADIUS, cv, i * 3);
			FloatBuffer fb = ByteBuffer.allocateDirect(Math.max(4, cv.length * 4)).order(ByteOrder.nativeOrder()).asFloatBuffer();
			fb.put(cv).position(0);
			dotCount = lat.length;
			dotVerts = fb;										// publish last (the GL thread reads it)
		}


	//////////
	// Zone rings:  the lock and the ramp's outer edge of every zone in the registry (a merged
	// zone:  each city's disc, and its ramp edge), each a small circle on the sphere (24 segments:
	// thousands of zones), packed into GL_LINES buffers.  Called by MainActivity once the
	// registry exists; safe before the surface (the buffers are data).
	//////
		public void setZones(SolTime.Registry r)
		{
			int      n    = r.count();
			double[] lat  = new double[n], lon = new double[n], lock = new double[n], edge = new double[n];
			int      nr   = 0;
			for (int i = 0; i < n; ++i)
			{
				lat[i]  = r.zone(i).latDeg;
				lon[i]  = r.zone(i).lonDegE;
				lock[i] = r.lockKm(i);
				edge[i] = r.lockKm(i) + r.rampKm(i);
			}
			FloatBuffer locks = iRings(lat, lon, lock);
			FloatBuffer ramps = iRings(lat, lon, edge);
			lockVertCount = locks.limit() / 3;
			rampVertCount = ramps.limit() / 3;
			rampVerts     = ramps;
			lockVerts     = locks;								// publish last (the GL thread reads it)
		}

		private static FloatBuffer iRings(double[] lats, double[] lons, double[] radiiKm)
		{
			final int SEG = 24;
			float[]   v   = new float[lats.length * SEG * 2 * 3];
			int       p   = 0;

			for (int b = 0; b < lats.length; ++b)
			{
				double theta = radiiKm[b] / 6371.0;					// angular radius on the sphere

				// Orthonormal frame around the center vector
				float[] c = new float[3];
				llToXyz(lats[b], lons[b], 1.0f, c, 0);
				double ax = 0.0, ay = 1.0, az = 0.0;
				if (Math.abs(c[1]) > 0.9)	{ ax = 1.0;  ay = 0.0; }
				double e1x = ay * c[2] - az * c[1], e1y = az * c[0] - ax * c[2], e1z = ax * c[1] - ay * c[0];
				double n   = Math.sqrt(e1x * e1x + e1y * e1y + e1z * e1z);
				e1x /= n;  e1y /= n;  e1z /= n;
				double e2x = c[1] * e1z - c[2] * e1y, e2y = c[2] * e1x - c[0] * e1z, e2z = c[0] * e1y - c[1] * e1x;

				double ct = Math.cos(theta), st = Math.sin(theta);
				float  px = 0.0f, py = 0.0f, pz = 0.0f;
				for (int s = 0; s <= SEG; ++s)
				{
					double phi = 2.0 * Math.PI * s / (double)SEG;
					double cp = Math.cos(phi), sp = Math.sin(phi);
					float  x  = (float)((c[0] * ct + (e1x * cp + e2x * sp) * st) * CITY_RADIUS);
					float  y  = (float)((c[1] * ct + (e1y * cp + e2y * sp) * st) * CITY_RADIUS);
					float  z  = (float)((c[2] * ct + (e1z * cp + e2z * sp) * st) * CITY_RADIUS);
					if (s > 0)	{ v[p++] = px;  v[p++] = py;  v[p++] = pz;  v[p++] = x;  v[p++] = y;  v[p++] = z; }
					px = x;  py = y;  pz = z;
				}
			}

			FloatBuffer fb = ByteBuffer.allocateDirect(Math.max(4, v.length * 4)).order(ByteOrder.nativeOrder()).asFloatBuffer();
			fb.put(v).position(0);
			fb.limit(v.length);
			return fb;
		}


	//////////
	// Texture:  assets/earth.jpg (equirectangular, e.g. NASA Blue Marble 2048x1024).
	// Fallback if missing:  a procedural ocean with ice caps and a graticule -- plain but alive.
	//////
		private Bitmap iLoadEarthBitmap()
		{
			try (InputStream in = context.getAssets().open("earth.jpg"))
			{
				Bitmap b = BitmapFactory.decodeStream(in);
				if (b != null)	return b;
			}
			catch (Exception ignored) { }

			Bitmap b = Bitmap.createBitmap(1024, 512, Bitmap.Config.ARGB_8888);
			Canvas c = new Canvas(b);
			Paint  p = new Paint(Paint.ANTI_ALIAS_FLAG);
			p.setColor(0xFF0A2A55);		c.drawRect(0, 0, 1024, 512, p);					// ocean
			p.setColor(0xFFDDEEFF);		c.drawRect(0, 0, 1024, 28, p);					// ice caps
										c.drawRect(0, 484, 1024, 512, p);
			p.setColor(0xFF1C4170);														// a 30-degree graticule
			for (int i = 1; i < 12; ++i)	c.drawLine(i * 1024.0f / 12.0f, 0.0f, i * 1024.0f / 12.0f, 512.0f, p);
			for (int i = 1; i < 6;  ++i)	c.drawLine(0.0f, i * 512.0f / 6.0f, 1024.0f, i * 512.0f / 6.0f, p);
			return b;
		}

		private int iCreateTexture()
		{
			Bitmap bmp   = iLoadEarthBitmap();
			int[]  ids   = new int[1];
			GLES20.glGenTextures(1, ids, 0);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, ids[0]);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR_MIPMAP_LINEAR);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S,     GLES20.GL_REPEAT);
			GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T,     GLES20.GL_CLAMP_TO_EDGE);
			GLUtils.texImage2D(GLES20.GL_TEXTURE_2D, 0, bmp, 0);
			GLES20.glGenerateMipmap(GLES20.GL_TEXTURE_2D);
			bmp.recycle();
			return ids[0];
		}


	//////////
	// Program plumbing
	//////
		private static int iCompile(int type, String src)
		{
			int sh = GLES20.glCreateShader(type);
			GLES20.glShaderSource(sh, src);
			GLES20.glCompileShader(sh);
			return sh;
		}

		private static int iProgram(String vs, String fs)
		{
			int prog = GLES20.glCreateProgram();
			GLES20.glAttachShader(prog, iCompile(GLES20.GL_VERTEX_SHADER,   vs));
			GLES20.glAttachShader(prog, iCompile(GLES20.GL_FRAGMENT_SHADER, fs));
			GLES20.glLinkProgram(prog);
			return prog;
		}


	//////////
	// GLSurfaceView.Renderer
	//////
		@Override
		public void onSurfaceCreated(GL10 unused, EGLConfig config)
		{
			GLES20.glClearColor(0.02f, 0.02f, 0.06f, 1.0f);
			GLES20.glEnable(GLES20.GL_DEPTH_TEST);
			GLES20.glEnable(GLES20.GL_CULL_FACE);
			GLES20.glCullFace(GLES20.GL_BACK);

			iBuildSphere();
			texEarth  = iCreateTexture();

			earthProg = iProgram(EARTH_VS, EARTH_FS);
			eaPos     = GLES20.glGetAttribLocation (earthProg, "aPos");
			eaUv      = GLES20.glGetAttribLocation (earthProg, "aUv");
			euMvp     = GLES20.glGetUniformLocation(earthProg, "uMvp");
			euSun     = GLES20.glGetUniformLocation(earthProg, "uSunDir");
			euTex     = GLES20.glGetUniformLocation(earthProg, "uTex");

			pointProg = iProgram(POINT_VS, POINT_FS);
			paPos     = GLES20.glGetAttribLocation (pointProg, "aPos");
			puMvp     = GLES20.glGetUniformLocation(pointProg, "uMvp");
			puColor   = GLES20.glGetUniformLocation(pointProg, "uColor");
			puSize    = GLES20.glGetUniformLocation(pointProg, "uSize");

			lineProg  = iProgram(LINE_VS, LINE_FS);
			laPos     = GLES20.glGetAttribLocation (lineProg, "aPos");
			luMvp     = GLES20.glGetUniformLocation(lineProg, "uMvp");
			luColor   = GLES20.glGetUniformLocation(lineProg, "uColor");
		}

		@Override
		public void onSurfaceChanged(GL10 unused, int width, int height)
		{
			GLES20.glViewport(0, 0, width, height);
			float aspect = (float)width / (float)Math.max(1, height);
			Matrix.perspectiveM(proj, 0, 45.0f, aspect, 0.1f, 20.0f);
		}

		@Override
		public void onDrawFrame(GL10 unused)
		{
			GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT | GLES20.GL_DEPTH_BUFFER_BIT);

			// Model = Rx(pitch) * Ry(yaw);  camera fixed on +Z looking at the origin
			Matrix.setRotateM(tmpA, 0, pitchDeg, 1.0f, 0.0f, 0.0f);
			Matrix.setRotateM(tmpB, 0, yawDeg,   0.0f, 1.0f, 0.0f);
			Matrix.multiplyMM(model, 0, tmpA, 0, tmpB, 0);
			Matrix.setLookAtM(view, 0, 0.0f, 0.0f, dist, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
			Matrix.multiplyMM(tmpA, 0, view, 0, model, 0);
			Matrix.multiplyMM(mvp,  0, proj, 0, tmpA,  0);

			// Reticle pick:  the earth-fixed point facing the camera is M^-1 * (0,0,1)
			Matrix.invertM(inv, 0, model, 0);
			vecIn[0] = 0.0f;  vecIn[1] = 0.0f;  vecIn[2] = 1.0f;  vecIn[3] = 0.0f;
			Matrix.multiplyMV(vec, 0, inv, 0, vecIn, 0);
			double vy = Math.max(-1.0, Math.min(1.0, vec[1]));
			centerLat = Math.toDegrees(Math.asin(vy));
			centerLon = Math.toDegrees(Math.atan2(vec[0], vec[2]));

			// The real sun, earth-fixed, right now -> the terminator is live
			SolTime.subsolarUnix(System.currentTimeMillis() / 1000.0, sunLL);
			llToXyz(sunLL[0], sunLL[1], 1.0f, sun, 0);

			// Earth
			GLES20.glUseProgram(earthProg);
			GLES20.glUniformMatrix4fv(euMvp, 1, false, mvp, 0);
			GLES20.glUniform3f(euSun, sun[0], sun[1], sun[2]);
			GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
			GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, texEarth);
			GLES20.glUniform1i(euTex, 0);
			sphereVerts.position(0);
			GLES20.glVertexAttribPointer(eaPos, 3, GLES20.GL_FLOAT, false, 20, sphereVerts);
			sphereVerts.position(3);
			GLES20.glVertexAttribPointer(eaUv,  2, GLES20.GL_FLOAT, false, 20, sphereVerts);
			GLES20.glEnableVertexAttribArray(eaPos);
			GLES20.glEnableVertexAttribArray(eaUv);
			GLES20.glDrawElements(GLES20.GL_TRIANGLES, indexCount, GLES20.GL_UNSIGNED_SHORT, sphereIndex);
			GLES20.glDisableVertexAttribArray(eaPos);
			GLES20.glDisableVertexAttribArray(eaUv);

			// Zone rings (spec §8):  the lock (where the time is exactly the city's), and the
			// ramp's outer edge (where the city's clock has blended back into the field)
			FloatBuffer locks = lockVerts, ramps = rampVerts;
			if (locks != null && ramps != null)
			{
				GLES20.glUseProgram(lineProg);
				GLES20.glUniformMatrix4fv(luMvp, 1, false, mvp, 0);
				GLES20.glEnableVertexAttribArray(laPos);
				GLES20.glUniform4f(luColor, 0.20f, 0.40f, 0.48f, 1.0f);
				ramps.position(0);
				GLES20.glVertexAttribPointer(laPos, 3, GLES20.GL_FLOAT, false, 12, ramps);
				GLES20.glDrawArrays(GLES20.GL_LINES, 0, rampVertCount);
				GLES20.glUniform4f(luColor, 0.35f, 0.75f, 0.85f, 1.0f);
				locks.position(0);
				GLES20.glVertexAttribPointer(laPos, 3, GLES20.GL_FLOAT, false, 12, locks);
				GLES20.glDrawArrays(GLES20.GL_LINES, 0, lockVertCount);
				GLES20.glDisableVertexAttribArray(laPos);
			}

			// City dots
			GLES20.glUseProgram(pointProg);
			GLES20.glUniformMatrix4fv(puMvp, 1, false, mvp, 0);
			GLES20.glEnableVertexAttribArray(paPos);
			FloatBuffer dots = dotVerts;
			if (dots != null)
			{
				GLES20.glUniform4f(puColor, 1.0f, 0.85f, 0.25f, 1.0f);
				GLES20.glUniform1f(puSize, 9.0f);
				dots.position(0);
				GLES20.glVertexAttribPointer(paPos, 3, GLES20.GL_FLOAT, false, 12, dots);
				GLES20.glDrawArrays(GLES20.GL_POINTS, 0, dotCount);
			}

			// You-are-here marker (red, bigger), once GPS has a fix
			double ulat = userLat, ulon = userLon;
			if (!Double.isNaN(ulat))
			{
				if (userVert == null || ulat != userVertLat || ulon != userVertLon)
				{
					float[] uv3 = new float[3];
					llToXyz(ulat, ulon, CITY_RADIUS + 0.002f, uv3, 0);
					userVert = ByteBuffer.allocateDirect(12).order(ByteOrder.nativeOrder()).asFloatBuffer();
					userVert.put(uv3).position(0);
					userVertLat = ulat;  userVertLon = ulon;
				}
				GLES20.glUniform4f(puColor, 1.0f, 0.2f, 0.15f, 1.0f);
				GLES20.glUniform1f(puSize, 16.0f);
				GLES20.glVertexAttribPointer(paPos, 3, GLES20.GL_FLOAT, false, 12, userVert);
				GLES20.glDrawArrays(GLES20.GL_POINTS, 0, 1);
			}
			GLES20.glDisableVertexAttribArray(paPos);
		}


	//////////
	// Aim the globe at a lat/lon (used to center on the first GPS fix)
	//////
		public void lookAt(double latDeg, double lonDeg)
		{
			pitchDeg = (float)latDeg;
			yawDeg   = (float)-lonDeg;
		}
}
