package edu.gmu.cs.CirclsClient;

import android.content.Context;

import com.obd.infrared.InfraRed;
import com.obd.infrared.log.Logger;
import com.obd.infrared.log.LogToConsole;
import com.obd.infrared.patterns.PatternAdapter;
import com.obd.infrared.patterns.PatternConverter;
import com.obd.infrared.patterns.PatternType;
import com.obd.infrared.transmit.TransmitterType;

public class TxHandler {
    private static final String TAG = "TxHandler";
    private static final int IR_CARRIER = 38400;

    private InfraRed mInfraRed;
    private PatternAdapter patternAdapter;

    static { System.loadLibrary("native-lib"); }
    private native int[] GetNAKPattern(int id);

    // setup CIR device
    public void setup(Context context) {
        Logger log = new LogToConsole(TAG);
        mInfraRed = new InfraRed(context, log);
        TransmitterType transmitterType = mInfraRed.detect();
        mInfraRed.createTransmitter(transmitterType);
        patternAdapter = new PatternAdapter(log, transmitterType);
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

    // convert ID to IR pattern and send
    public void sendNAK(int id) {
        if(mInfraRed != null) {
            int data[] = GetNAKPattern(id);
            mInfraRed.transmit(patternAdapter.createTransmitInfo(
                    new PatternConverter(PatternType.Cycles, IR_CARRIER, data)));
        }
    }

}
