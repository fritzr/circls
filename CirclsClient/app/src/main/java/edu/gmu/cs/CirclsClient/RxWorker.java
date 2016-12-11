package edu.gmu.cs.CirclsClient;

import android.util.Log;

import java.util.Arrays;

import org.opencv.core.Mat;

public class RxWorker implements Runnable {
    private static final String TAG = "RxWorker";

    private Mat mat;

    static { System.loadLibrary("native-lib"); }
    private native char[] ImageProcessor(long inputFrame);

    public RxWorker(Mat mat) {
        this.mat = mat;
    }

    @Override
    public void run() {
        char data[] = ImageProcessor(mat.getNativeObjAddr());
        String row = Arrays.toString(data);
        Log.d(TAG, row);
    }
}
