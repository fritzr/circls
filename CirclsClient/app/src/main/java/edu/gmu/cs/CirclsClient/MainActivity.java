package edu.gmu.cs.CirclsClient;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.design.widget.Snackbar;
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


public class MainActivity extends AppCompatActivity implements View.OnClickListener, CvCameraViewListener2 {

    private LogToEditText        log;
    private InfraRed             infraRed;
    private PatternAdapter       patternAdapter;
    private CameraBridgeViewBase mOpenCvCameraView;

    private static final int CAMERA_PERMS = 0xbeef;
    protected boolean haveCamera = false;

    private static final int IR_PERMS = 0xbadd;
    protected boolean haveIR = false;

    static {
        System.loadLibrary("native-lib");
    }

    protected void getPermissions() {
        // Here, thisActivity is the current activity
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.READ_CONTACTS)
                != PackageManager.PERMISSION_GRANTED) {

            // Should we show an explanation?
            if (ActivityCompat.shouldShowRequestPermissionRationale(this,
                    Manifest.permission.READ_CONTACTS)) {

                // Show an explanation to the user *asynchronously* -- don't block
                // this thread waiting for the user's response! After the user
                // sees the explanation, try again to request the permission.

            } else {

                // No explanation needed, we can request the permission.

                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.CAMERA, Manifest.permission.TRANSMIT_IR,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE,
                        Manifest.permission.READ_EXTERNAL_STORAGE},
                        CAMERA_PERMS);

                // MY_PERMISSIONS_REQUEST_READ_CONTACTS is an
                // app-defined int constant. The callback method gets the
                // result of the request.
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        switch (requestCode) {
            case CAMERA_PERMS: {
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                    // permission was granted, yay! Do the
                    // contacts-related task you need to do.
                    haveCamera = true;
                    // setup camera
                    mOpenCvCameraView = (CameraBridgeViewBase) findViewById(R.id.surface_view);
                    mOpenCvCameraView.setVisibility(CameraBridgeViewBase.VISIBLE);
                    mOpenCvCameraView.setCvCameraViewListener(this);
                } else {

                    // permission denied, boo! Disable the
                    // functionality that depends on this permission.
                    haveCamera = false;
                }
                return;
            }
            case IR_PERMS: {
                if (grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    haveIR = true;

                    // setup InfraRed
                    infraRed = new InfraRed(this, log);
                    TransmitterType transmitterType = infraRed.detect();
                    log.log("TransmitterType: " + transmitterType);
                    infraRed.createTransmitter(transmitterType);
                    patternAdapter = new PatternAdapter(log, transmitterType);
                } else {
                    haveIR = false;
                }
                return;
            }

            // other 'case' lines to check for other
            // permissions this app might request
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_DISPLAY);


        Button transmitButton = (Button) this.findViewById(R.id.transmit_button);
        transmitButton.setOnClickListener(this);

        // Log messages to EditText
        EditText console = (EditText) this.findViewById(R.id.console);
        log = new LogToEditText(console, "CIRCLS");

        getPermissions ();
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
        if (infraRed != null && haveIR)
            infraRed.start();
        if (!haveCamera)
            return;

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
        Snackbar.make(v, "Sending IR...", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show();
        log.clear();
        if (infraRed != null && haveIR)
        infraRed.transmit(patternAdapter.createTransmitInfo(
                new PatternConverter(PatternType.Cycles, 38000,494,114,38,38,38,38,38,38,38,38,38,38,38,114,38,1)));
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (infraRed != null)
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
