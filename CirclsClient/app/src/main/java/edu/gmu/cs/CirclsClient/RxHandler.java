package edu.gmu.cs.CirclsClient;

import android.util.Log;
import android.view.View;
import android.opengl.GLES20;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraGLSurfaceView;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;

import java.nio.ByteBuffer;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class RxHandler implements CameraGLSurfaceView.CameraTextureListener {
    private static final String TAG = "RxHandler";
    private static final int NUMBER_OF_CORES = Runtime.getRuntime().availableProcessors();
    private static final long THREAD_KEEPALIVE = 1000; // ms

    private final ThreadPoolExecutor mWorkers;
    private BaseLoaderCallback mLoaderCallback;
    private MessageHandler mDisplay;
    private CameraGLSurfaceView mView;

    // jni
    static { System.loadLibrary("native-lib"); }
    private native char[] FrameProcessor(int width, int height, ByteBuffer data);

    class Worker implements Runnable {
        private ByteBuffer pixels;
        private int width;
        private int height;

        Worker(int width, int height, ByteBuffer pixels) {
            this.width = width;
            this.height = height;
            this.pixels = pixels;
        }

        @Override
        public void run() {
            char[] text = FrameProcessor(width, height, pixels);

            if (text.length > 0) {
                mDisplay.update(0, String.valueOf(text));
            }
        }
    }

    public RxHandler() {
        // only handle one frame at a time
        mWorkers = new ThreadPoolExecutor(
                1,
                1,
                THREAD_KEEPALIVE,
                TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>()
        );
    }

    public void setup(View view, MessageHandler display) {
        // setup display
        mView = (CameraGLSurfaceView) view;
        mView.setMaxCameraPreviewSize(960, 720);
        mView.setCameraTextureListener(this);

        // setup callback for OpenCV loader
        mLoaderCallback = new BaseLoaderCallback(view.getContext()) {
            @Override
            public void onManagerConnected(int status) {
                switch (status) {
                    case LoaderCallbackInterface.SUCCESS:
                        Log.d(TAG, "OpenCV loaded successfully");
                        mView.enableView();
                        break;
                    default:
                        super.onManagerConnected(status);
                        break;
                }
            }
        };

        mDisplay = display;
    }

    public void start() {
        if (OpenCVLoader.initDebug()) {
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }

    public void stop() {
        mView.disableView();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        Log.d(TAG, "Preview (" + width + "," + height + ")");
    }

    @Override
    public void onCameraViewStopped() {
        mWorkers.purge();
    }

    @Override
    public boolean onCameraTexture(int texIn, int texOut, int width, int height) {

        ByteBuffer pixels = ByteBuffer.allocateDirect(width * height * 4);
        GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixels);
        mWorkers.execute(new Worker(width, height, pixels));

        // output isn't modified
        return false;
    }
}
