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

import java.util.BitSet;

public class MainActivity extends AppCompatActivity implements MessageHandler {
    private static final String TAG = "CIRCLS Client";
    private static final int CAMERA_PERMS = 0xbeef;
    private static final int IR_PERMS = 0xbadd;

    private LogToEditText display;
    private RxHandler rx = new RxHandler();
    private TxHandler tx = new TxHandler();

    // sliding window
    private static final int MAX_ID = 256;
    private static final int WINDOW_SIZE = MAX_ID / 2;
    private BitSet window = new BitSet(MAX_ID);
    private int tail = 0;

    @Override
    public void update(final Integer id, final String msg) {
        runOnUiThread(new Runnable() {
            public void run() {
                display.append(id + ":" + msg);

                // if the message is in the window
                if ((id - tail + MAX_ID) % MAX_ID <= WINDOW_SIZE) {
                    // flag received
                    window.set(id);

                    // slide window
                    while (window.get(tail)) {
                        window.clear(tail++);
                    }

                    // NAK missing id
                    tx.sendNAK(tail);
                    display.append(tail + ":NAK");
                }
            }
        });
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
        display = new LogToEditText(console, TAG);

        // temporary
        console.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                tx.sendNAK(tail);
                display.append(tail + ":NAK");
                tail = (tail + 1) % MAX_ID;
            }
        });

        // request camera & IR
        getPermissions();
    }

    @Override
    protected void onResume() {
        super.onResume();
        rx.start();
        tx.start();
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
