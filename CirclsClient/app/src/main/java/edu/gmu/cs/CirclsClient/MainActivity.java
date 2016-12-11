package edu.gmu.cs.CirclsClient;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.os.Process;

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
import org.opencv.core.Mat;

import java.util.Arrays;


public class MainActivity extends AppCompatActivity implements View.OnClickListener, CvCameraViewListener2 {

    private LogToEditText        log;
    private InfraRed             mInfraRed;
    private PatternAdapter       patternAdapter;
    private CameraBridgeViewBase mCameraView;
    private boolean b = false;

    private static final int CAMERA_PERMS = 0xbeef;
    private static final int IR_PERMS = 0xbadd;

    static {
        System.loadLibrary("native-lib");
    }

    protected void setupCamera() {
        mCameraView = (CameraBridgeViewBase) findViewById(R.id.surface_view);
        mCameraView.setVisibility(CameraBridgeViewBase.VISIBLE);
        mCameraView.setCvCameraViewListener(this);
        mCameraView.enableFpsMeter();
    }

    protected void startCamera() {
        if (mCameraView != null) {
            mCameraView.enableView();
        }
    }

    protected void stopCamera() {
        if (mCameraView != null) {
            mCameraView.disableView();
        }
    }

    protected void setupIR() {
        mInfraRed = new InfraRed(this, log);
        TransmitterType transmitterType = mInfraRed.detect();
        mInfraRed.createTransmitter(transmitterType);
        patternAdapter = new PatternAdapter(log, transmitterType);
        log.log("IR " + transmitterType);
    }

    protected void startIR() {
        if (mInfraRed != null) {
            mInfraRed.start();
        }
    }

    protected void stopIR() {
        if (mInfraRed != null) {
            mInfraRed.stop();
        }
    }

    protected void getPermissions() {
        // request Camera permissions if needed
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                new String[]{ Manifest.permission.CAMERA },
                CAMERA_PERMS);
        } else {
            setupCamera();
        }

        // request IR permissions if needed
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.TRANSMIT_IR) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                new String[]{ Manifest.permission.TRANSMIT_IR },
                IR_PERMS);
        } else {
            setupIR();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        switch (requestCode) {
            case CAMERA_PERMS: {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    log.log("Camera granted");
                    setupCamera();
                } else {
                    log.log("Camera denied");
                    stopCamera();
                }
                break;
            }

            case IR_PERMS: {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    log.log("IR granted");
                    setupIR();
                } else {
                    log.log("IR denied");
                    stopIR();
                }
                break;
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_DISPLAY);

        Button transmitButton = (Button) findViewById(R.id.transmit_button);
        transmitButton.setOnClickListener(this);

        // Log messages to EditText
        EditText console = (EditText) findViewById(R.id.console);
        log = new LogToEditText(console, "CIRCLS");

        getPermissions();
    }

    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
        @Override
        public void onManagerConnected(int status) {
            switch (status) {
                case LoaderCallbackInterface.SUCCESS:
                    log.log("OpenCV loaded successfully");
                    startCamera();
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

        if (OpenCVLoader.initDebug()) {
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }

        startIR();
    }

    @Override
    public void onClick(View v) {
        log.clear();
        b = true;
        if (mInfraRed != null) {
            mInfraRed.transmit(patternAdapter.createTransmitInfo(
                    new PatternConverter(PatternType.Cycles, 38000, 494, 114, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 38, 114, 38, 1)));
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopIR();
        stopCamera();

        if (log != null) {
            log.destroy();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        stopIR();
        stopCamera();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
        log.log("Preview (" + width + "," + height + ")");
    }

    @Override
    public void onCameraViewStopped() {
    }

    @Override
    public Mat onCameraFrame(CvCameraViewFrame inputFrame) {
        Mat mat = inputFrame.rgba();

        if (b) {
            b = false;
            char data[] = ImageProcessor(mat.getNativeObjAddr());
            String row = Arrays.toString(data);
            log.log(row);
        }

        return mat;
    }

    private native char[] ImageProcessor(long inputFrame);
}
