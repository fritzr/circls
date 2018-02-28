package edu.gmu.cs.CirclsClient;

import android.util.Log;
import android.view.View;
import android.opengl.GLES20;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraGLSurfaceView;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;

import java.nio.ByteBuffer;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;


public class RxHandler implements CameraGLSurfaceView.CameraTextureListener {
    private static final String TAG = "RxHandler";

    private BlockingQueue<ByteBuffer> mFrameQueue = new LinkedBlockingQueue<>();
    private BaseLoaderCallback mLoaderCallback;
    private MessageHandler mDisplay;
    private CameraGLSurfaceView mView;

    // jni
    static { System.loadLibrary("native-lib"); }
    private native void FrameSize(int width, int height);
    private native char[] FrameProcessor(ByteBuffer data);

    class Consumer implements Runnable {
        @Override
        public void run() {
            while (true) {
                try {
                    char[] text = FrameProcessor(mFrameQueue.take());

                    if (text.length > 1) {
                        mDisplay.update(Integer.valueOf(text[0]), String.valueOf(text).substring(1));
                    }
                } catch (InterruptedException e) {
                }
            }
        }
    }

    public void setup(View view, MessageHandler display) {
        mDisplay = display;
        new Thread(new Consumer()).start();

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
        FrameSize(width, height);
        Log.d(TAG, "Preview (" + width + "," + height + ")");
    }

    @Override
    public void onCameraViewStopped() {
        mFrameQueue.clear();
    }

    @Override
    public boolean onCameraTexture(int texIn, int texOut, int width, int height) {
        // queue frame for processing
        ByteBuffer pixels = ByteBuffer.allocateDirect(width * height * 4);
        GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, pixels);
        mFrameQueue.add(pixels);

        // output isn't modified
        return false;
    }
}
