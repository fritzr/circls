package edu.gmu.cs.CirclsClient;

import android.os.Bundle;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;

import com.obd.infrared.InfraRed;
import com.obd.infrared.log.LogToEditText;
import com.obd.infrared.patterns.PatternAdapter;
import com.obd.infrared.patterns.PatternConverter;
import com.obd.infrared.patterns.PatternType;
import com.obd.infrared.transmit.TransmitterType;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewListener2;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewFrame;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.imgproc.Imgproc;


public class MainActivity extends AppCompatActivity implements View.OnClickListener, CvCameraViewListener2 {

    private LogToEditText        log;
    private InfraRed             infraRed;
    private PatternAdapter       patternAdapter;
    private CameraBridgeViewBase mOpenCvCameraView;

    static {
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button transmitButton = (Button) this.findViewById(R.id.transmit_button);
        transmitButton.setOnClickListener(this);

        // Log messages to EditText
        EditText console = (EditText) this.findViewById(R.id.console);
        log = new LogToEditText(console, "CIRCLS");

        // setup InfraRed
        infraRed = new InfraRed(this, log);
        TransmitterType transmitterType = infraRed.detect();
        log.log("TransmitterType: " + transmitterType);
        infraRed.createTransmitter(transmitterType);
        patternAdapter = new PatternAdapter(log, transmitterType);

        // setup camera
        mOpenCvCameraView = (CameraBridgeViewBase) findViewById(R.id.surface_view);
        mOpenCvCameraView.setVisibility(CameraBridgeViewBase.VISIBLE);
        mOpenCvCameraView.setCvCameraViewListener(this);
    }

    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
        @Override
        public void onManagerConnected(int status) {
            switch (status) {
                case LoaderCallbackInterface.SUCCESS:
                    log.log("OpenCV: " + "OpenCV loaded successfully");
                    mOpenCvCameraView.enableView();
                    mOpenCvCameraView.enableFpsMeter();
                    break;
                default:
                    super.onManagerConnected(status);
                    break;
            }
        }
    };

    @Override
    protected void onResume() {
        super.onResume();
        infraRed.start();

        if (!OpenCVLoader.initDebug()) {
            log.log("OpenCV: " + "Internal OpenCV library not found. Using OpenCV Manager for initialization");
            OpenCVLoader.initAsync(OpenCVLoader.OPENCV_VERSION_3_1_0, this, mLoaderCallback);
        } else {
            log.log("OpenCV: " + "OpenCV library found inside package. Using it!");
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }


    @Override
    public void onClick(View v) {
        Snackbar.make(v, "Reading LED...", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show();

        Snackbar.make(v, "Sending IR...", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show();
        infraRed.transmit(patternAdapter.createTransmitInfo(
                new PatternConverter(PatternType.Cycles, 38000,494,114,38,38,38,38,38,38,38,38,38,38,38,114,38,1)));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        infraRed.stop();

        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();

        if (log != null) {
            log.destroy();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        log.log("OpenCV: " + "Preview (" + width + "," + height + ")");
    }

    @Override
    public void onCameraViewStopped() {
    }

    @Override
    public Mat onCameraFrame(CvCameraViewFrame inputFrame) {
        Mat mat = inputFrame.rgba();
        int rgb[] = ImageProcessor(mat.getNativeObjAddr());
        log.log("RGB: (" + rgb[0] + "," + rgb[1] + "," + rgb[2] + ")");
        return mat;
    }

    private native int[] ImageProcessor(long inputFrame);
}
