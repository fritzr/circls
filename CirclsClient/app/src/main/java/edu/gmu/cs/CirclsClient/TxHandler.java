package edu.gmu.cs.CirclsClient;

import android.content.Context;
import android.util.Log;

import com.obd.infrared.InfraRed;
import com.obd.infrared.log.LogToEditText;
import com.obd.infrared.patterns.PatternAdapter;
import com.obd.infrared.patterns.PatternConverter;
import com.obd.infrared.patterns.PatternType;
import com.obd.infrared.transmit.TransmitterType;

public class TxHandler {
    private static final String TAG = "TxHandler";
    private static final int IR_CARRIER = 33000;

    private InfraRed mInfraRed;
    private PatternAdapter patternAdapter;

    public void setup(Context context, LogToEditText log) {
        mInfraRed = new InfraRed(context, log);
        TransmitterType transmitterType = mInfraRed.detect();
        mInfraRed.createTransmitter(transmitterType);
        patternAdapter = new PatternAdapter(log, transmitterType);
        Log.d(TAG, transmitterType.toString());
    }

    public void start() {
        if (mInfraRed != null) {
            mInfraRed.start();
        }
    }

    public void stop() {
        if (mInfraRed != null) {
            mInfraRed.stop();
        }
    }

    public void send(int data[]) {
        if(mInfraRed != null) {
            mInfraRed.transmit(patternAdapter.createTransmitInfo(
                    new PatternConverter(PatternType.Cycles, IR_CARRIER, data)));
        }
    }

}
