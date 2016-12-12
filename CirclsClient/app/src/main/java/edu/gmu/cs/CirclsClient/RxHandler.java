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

    private CameraBridgeViewBase mCameraView;
    private BaseLoaderCallback mLoaderCallback;
    private final ThreadPoolExecutor mRxWorkers;

    public RxHandler() {
        // use multiple threads to handle Rx images
        mRxWorkers = new ThreadPoolExecutor(
                NUMBER_OF_CORES * 2,
                NUMBER_OF_CORES * 2,
                THREAD_KEEPALIVE,
                TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>()
        );
    }

    public void setup(View view) {
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
        mRxWorkers.purge();
    }

    @Override
    public Mat onCameraFrame(CameraBridgeViewBase.CvCameraViewFrame inputFrame) {
        Mat mat = inputFrame.rgba();

        // queue up frame for processing
        mRxWorkers.execute(new RxWorker(mat));

        return mat;
    }

}
