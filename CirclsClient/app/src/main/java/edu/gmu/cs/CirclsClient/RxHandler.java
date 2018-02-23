package edu.gmu.cs.CirclsClient;

import android.util.Log;
import android.view.View;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraGLSurfaceView;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;

public class RxHandler implements CameraGLSurfaceView.CameraTextureListener {
    private static final String TAG = "RxHandler";

    private BaseLoaderCallback mLoaderCallback;
    private MessageHandler mDisplay;
    private CameraGLSurfaceView mView;

    // jni
    static { System.loadLibrary("native-lib"); }
    private native char[] FrameProcessor(int width, int height);


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
    }

    @Override
    public boolean onCameraTexture(int texIn, int texOut, int width, int height) {
        char[] data = FrameProcessor(width, height);
        if (data.length > 0) {
            mDisplay.update(data.length, String.valueOf(data));
        }

        // output isn't modified
        return false;
    }
}
