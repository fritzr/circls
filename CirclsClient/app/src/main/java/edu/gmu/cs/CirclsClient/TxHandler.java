package edu.gmu.cs.CirclsClient;

import android.content.Context;

import com.obd.infrared.InfraRed;
import com.obd.infrared.log.Logger;
import com.obd.infrared.log.LogToConsole;
import com.obd.infrared.patterns.PatternAdapter;
import com.obd.infrared.patterns.PatternConverter;
import com.obd.infrared.patterns.PatternType;
import com.obd.infrared.transmit.TransmitterType;

import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

public class TxHandler {
    private static final String TAG = "TxHandler";
    private static final int IR_CARRIER = 38400;
    private static final long THREAD_KEEPALIVE = 1000; // ms

    private final ThreadPoolExecutor mWorkers;

    private InfraRed mInfraRed;
    private PatternAdapter patternAdapter;

    static { System.loadLibrary("native-lib"); }
    private native int[] GetNAKPattern(int id);

    class Worker implements Runnable {
        private int id;
        Worker(int id) {
            this.id = id;
        }

        @Override
        public void run() {
            if(mInfraRed != null) {
                int data[] = GetNAKPattern(id);
                mInfraRed.transmit(patternAdapter.createTransmitInfo(
                        new PatternConverter(PatternType.Cycles, IR_CARRIER, data)));
            }
        }
    }

    public TxHandler() {
        // run one TX thread at a time
        mWorkers = new ThreadPoolExecutor(
                1,
                1,
                THREAD_KEEPALIVE,
                TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>()
        );
    }

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
        mWorkers.execute(new Worker(id));
    }

}
