package edu.gmu.cs.CirclsClient;

import android.util.Log;
import android.view.View;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.Mat;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class RxHandler implements CameraBridgeViewBase.CvCameraViewListener2 {
    private static final String TAG = "RxHandler";
    private static final int NUMBER_OF_CORES = Runtime.getRuntime().availableProcessors();
    private static final long THREAD_KEEPALIVE = 1000; // ms

    private final ThreadPoolExecutor mWorkers;
    private CameraBridgeViewBase mCameraView;
    private BaseLoaderCallback mLoaderCallback;
    private MessageHandler mDisplay;

    static { System.loadLibrary("native-lib"); }
    private native char[] ImageProcessor(long inputFrame);

    class Worker implements Runnable {
        private Mat mat;
        Worker(Mat mat) {
            this.mat = mat;
        }

        @Override
        public void run() {
            char data[] = ImageProcessor(mat.getNativeObjAddr());
            mDisplay.update(data[0], String.valueOf(data));
        }
    }

    public RxHandler() {
        // use multiple threads to handle Rx images
        mWorkers = new ThreadPoolExecutor(
                NUMBER_OF_CORES * 2,
                NUMBER_OF_CORES * 2,
                THREAD_KEEPALIVE,
                TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>()
        );
    }

    public void setup(View view, MessageHandler display) {
        // setup display
        mCameraView = (CameraBridgeViewBase) view;
        mCameraView.setVisibility(CameraBridgeViewBase.VISIBLE);
        mCameraView.setCvCameraViewListener(this);
        mCameraView.enableFpsMeter();

        // setup callback for OpenCV loader
        mLoaderCallback = new BaseLoaderCallback(view.getContext()) {
            @Override
            public void onManagerConnected(int status) {
                switch (status) {
                    case LoaderCallbackInterface.SUCCESS:
                        Log.d(TAG, "OpenCV loaded successfully");
                        if (mCameraView != null) {
                            mCameraView.enableView();
                        }
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
        if (mCameraView != null && OpenCVLoader.initDebug()) {
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }

    public void stop() {
        if (mCameraView != null) {
            mCameraView.disableView();
        }
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        Log.d(TAG, "Preview (" + width + "," + height + ")");
    }

    @Override
    public void onCameraViewStopped() {
        mWorkers.purge();
    }

    // queue up each frame for processing
    @Override
    public Mat onCameraFrame(CameraBridgeViewBase.CvCameraViewFrame inputFrame) {
        Mat mat = inputFrame.rgba();
        mWorkers.execute(new Worker(mat));
        return mat;
    }
}
