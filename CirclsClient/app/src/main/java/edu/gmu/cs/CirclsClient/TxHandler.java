package edu.gmu.cs.CirclsClient;

import android.content.Context;

import com.obd.infrared.InfraRed;
import com.obd.infrared.log.Logger;
import com.obd.infrared.log.LogToConsole;
import com.obd.infrared.patterns.PatternAdapter;
import com.obd.infrared.patterns.PatternConverter;
import com.obd.infrared.patterns.PatternType;
import com.obd.infrared.transmit.TransmitterType;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;

public class TxHandler {
    private static final String TAG = "TxHandler";
    private static final int IR_CARRIER = 56000;
    private static final int PULSE_WIDTH = 8;
    private static final int IR_PACKET_SIZE = 34;

    private final BlockingQueue<Integer> mIdQueue = new LinkedBlockingQueue<>();

    private InfraRed mInfraRed;
    private PatternAdapter patternAdapter;

    class Consumer implements Runnable {
        @Override
        public void run() {
            while (true) {
                try {
                    int id = mIdQueue.take();
                    int[] data = GetNAKPattern(id);

                    mInfraRed.transmit(patternAdapter.createTransmitInfo(
                            new PatternConverter(PatternType.Cycles, IR_CARRIER, data)));
                } catch (InterruptedException e) {
                }
            }
        }
    }

    // setup CIR device
    public void setup(Context context) {
        Logger log = new LogToConsole(TAG);
        mInfraRed = new InfraRed(context, log);

        TransmitterType transmitterType = mInfraRed.detect();
        mInfraRed.createTransmitter(transmitterType);
        patternAdapter = new PatternAdapter(log, transmitterType);

        new Thread(new Consumer()).start();
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
        mIdQueue.add(id);
    }


    // generate NAK pattern from ID
    int[] GetNAKPattern(int id)
    {
        int[] ret = new int[IR_PACKET_SIZE];

        // magic + fcs
        id |= 0b10100000 << 8;

        // each bit is represented by a total of 4 pulses
        int i = 0;
        for (int b = 15; b >= 0; b--)
        {
            boolean set = ((1 << b) & id) != 0;
            ret[i++] = (set ? 3 : 1) * PULSE_WIDTH; // on
            ret[i++] = (set ? 1 : 3) * PULSE_WIDTH; // off
        }

        // tail
        ret[i++] = PULSE_WIDTH;
        ret[i] = PULSE_WIDTH;

        return ret;
    }

}
