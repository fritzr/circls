package edu.gmu.cs.CirclsClient;

import android.os.Bundle;
import android.os.Environment;
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
import com.obd.infrared.transmit.TransmitInfo;
import com.obd.infrared.transmit.TransmitterType;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.Mat;
import org.opencv.imgcodecs.Imgcodecs;
import org.opencv.imgproc.Imgproc;

import java.io.File;
import java.util.ArrayList;
import java.util.List;


public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private LogToEditText log;
    private InfraRed infraRed;
    private TransmitInfo[] patterns;

    File pictureDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES);
    String inputFileName = "Green";
    String inputExtension = "jpg";
    String inputDir = pictureDir.getAbsolutePath();
    String inputFilePath = inputDir + File.separator + inputFileName + "." + inputExtension;
    String outputDir = pictureDir.getAbsolutePath();
    String outputExtension = "png";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String TAG = "IR";

        Button transmitButton = (Button) this.findViewById(R.id.transmit_button);
        transmitButton.setOnClickListener(this);

        // Log messages print to EditText
        EditText console = (EditText) this.findViewById(R.id.console);
        log = new LogToEditText(console, TAG);

        infraRed = new InfraRed(this, log);
        // detect transmitter type
        TransmitterType transmitterType = infraRed.detect();
        log.log("TransmitterType: " + transmitterType);

        // initialize transmitter by type
        infraRed.createTransmitter(transmitterType);

        // initialize raw patterns
        List<PatternConverter> rawPatterns = new ArrayList<>();

        // Pentax AF
        rawPatterns.add(new PatternConverter(PatternType.Cycles, 38000,494,114,38,38,38,38,38,38,38,38,38,38,38,114,38,1));

        // adapt the patterns for the device that is used to transmit the patterns
        PatternAdapter patternAdapter = new PatternAdapter(log, transmitterType);

        patterns = new TransmitInfo[rawPatterns.size()];
        for (int i = 0; i < patterns.length; i++) {
            patterns[i] = patternAdapter.createTransmitInfo(rawPatterns.get(i));
        }

    }

    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
        @Override
        public void onManagerConnected(int status) {
            switch (status) {
                case LoaderCallbackInterface.SUCCESS:
                    log.log("OpenCV: " + "OpenCV loaded successfully");
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
            OpenCVLoader.initAsync(OpenCVLoader.OPENCV_VERSION_3_0_0, this, mLoaderCallback);
        } else {
            log.log("OpenCV: " + "OpenCV library found inside package. Using it!");
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }


    @Override
    public void onClick(View v) {
        Snackbar.make(v, "Sending IR...", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show();

        log.log("OpenCV: " + "loading " + inputFilePath + "...");
        Mat image = Imgcodecs.imread(inputFilePath);
        log.log("OpenCV: " + "width of " + inputFileName + ": " + image.width());
// if width is 0 then it did not read your image.

// for the canny edge detection algorithm, play with these to see different results
        int threshold1 = 70;
        int threshold2 = 100;

        Mat im_canny = new Mat();  // you have to initialize output image before giving it to the Canny method
        Imgproc.Canny(image, im_canny, threshold1, threshold2);
        String cannyFilename = outputDir + File.separator + inputFileName + "_canny-" + threshold1 + "-" + threshold2 + "." + outputExtension;
        log.log("OpenCV: " + "Writing " + cannyFilename);
        Imgcodecs.imwrite(cannyFilename, im_canny);

        for (int i = 0; i < patterns.length; i++) {
            TransmitInfo transmitInfo = patterns[i];
            infraRed.transmit(transmitInfo);
        }
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();

        infraRed.stop();

        if (log != null) {
            log.destroy();
        }
    }

}
