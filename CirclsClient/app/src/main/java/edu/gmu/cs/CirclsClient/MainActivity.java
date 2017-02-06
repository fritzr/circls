package edu.gmu.cs.CirclsClient;

import android.Manifest;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.view.View;
import android.widget.EditText;

import com.obd.infrared.log.LogToEditText;

import java.util.Random;

public class MainActivity extends AppCompatActivity implements View.OnClickListener, MessageHandler {
    private static final String TAG = "CIRCLS Client";
    private static final int CAMERA_PERMS = 0xbeef;
    private static final int IR_PERMS = 0xbadd;

    private LogToEditText display;
    private RxHandler rx = new RxHandler();
    private TxHandler tx = new TxHandler();

    @Override
    public void update(int id, String msg) {
        try {
            display.append(msg + '\n');
        } catch (Exception e) {}
    }

    protected void getPermissions() {
        // request Camera permissions if needed
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.CAMERA},
                    CAMERA_PERMS);
        } else {
            rx.setup(findViewById(R.id.surface_view), this);
        }

        // request IR permissions if needed
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.TRANSMIT_IR) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.TRANSMIT_IR},
                    IR_PERMS);
        } else {
            tx.setup(this);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        switch (requestCode) {
            case CAMERA_PERMS: {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    display.log("Camera granted");
                    rx.setup(findViewById(R.id.surface_view), this);
                } else {
                    display.log("Camera denied");
                    rx.stop();
                }
                break;
            }

            case IR_PERMS: {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    display.log("IR granted");
                    tx.setup(this);
                } else {
                    display.log("IR denied");
                    tx.stop();
                }
                break;
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // setup display
        EditText console = (EditText) findViewById(R.id.console);
        console.setOnClickListener(this);
        display = new LogToEditText(console, TAG);

        // request camera & IR
        getPermissions();
    }

    @Override
    protected void onResume() {
        super.onResume();
        rx.start();
        tx.start();
    }

    Random rnd = new Random(); // tmp
    @Override
    public void onClick(View v) {
        display.clear();
        int i = rnd.nextInt(256);
        display.log("Sending: " + i);
        tx.sendNAK(i);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        tx.stop();
        rx.stop();

        if (display != null) {
            display.destroy();
        }
    }

    @Override
    public void onPause() {
        super.onPause();
        tx.stop();
        rx.stop();
    }

}
