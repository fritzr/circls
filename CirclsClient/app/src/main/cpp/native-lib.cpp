#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "rscode-1.3/ecc.h"

using namespace std;
using namespace cv;


// takes an OpenCV Matrix of 3D pixels
// returns a single averaged column of 3D pixels
void flattenCols(Mat &mat, int32_t flat[][3]) {
    // get Mat properties
    int rows = mat.rows;
    int cols = mat.cols;
    uint8_t *data = (uint8_t *)mat.data;

    // initialize flat array
    memset(flat, 0, sizeof(int32_t) * rows);

    for (int i = 0; i < rows; i++) {

        // sum column values across the row
        for (int j = 0; j < cols; j++) {
            flat[i][0] += (*data++);
            flat[i][1] += (*data++);
            flat[i][2] += (*data++);
        }

        // average values for the row
        flat[i][0] /= cols;
        flat[i][1] = flat[i][1] / cols - 128;
        flat[i][2] = flat[i][2] / cols - 128;
    }
}


// takes an OpenCV Matrix of 3D pixels
// returns a single averaged row of 3D pixels
void flattenRows(Mat &mat, int32_t flat[][3]) {
    // get Mat properties
    int rows = mat.rows;
    int cols = mat.cols;
    uint8_t *data = (uint8_t *)mat.data;

    // initialize flat frame
    memset(flat, 0, sizeof(int32_t) * cols);

    // sum values for each column across all rows
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            flat[j][0] += (*data++);
            flat[j][1] += (*data++);
            flat[j][2] += (*data++);
        }
    }

    // average the values for each flat column
    for (int i = 0; i < cols; i++) {
        flat[i][0] /= rows;
        flat[i][1] = flat[i][1] / rows - 128;
        flat[i][2] = flat[i][2] / rows - 128;
    }
}


void detectSymbols(int32_t frame[][3], jchar symbols[], int pixels) {

    // convert ab numbers to RGBYW representation
    for (int i = 0; i < pixels; i++) {
        int L = frame[i][0];
        int a = frame[i][1] - 128;
        int b = frame[i][2] - 128;
        jchar c;

        // +a = red; -a = green; -b = blue; +b = yellow;
        if (a == b) {
            c = (L == 0) ? '0' : '1';
        }
        else if (abs(a) > abs(b)) {
            c = (a < 0) ? 'G' : 'R';
        }
        else {
            c = (b < 0) ? 'B' : 'Y';
        }

        symbols[i] = c;
    }

}


static int
decode_rs (uint8_t *encoded, size_t length)
{
    bool check_pass = true;
    int st;

    // Then decode the remainder of the message, in 255-byte chunks.
    // The input in 255 - NPAR byte data chunks, with NPAR parity bits after each
    // chunk, plus a possibly smaller chunk at the end, followed by a FCS.
    int left = length;
    int i = 0;
    int out_index = 0;
    int in_index = 0;
    while (left > 0)
    {
        int en_chunk = 255;
        if (left < en_chunk)
            en_chunk = left;

        decode_data (encoded + in_index, en_chunk);
        int syndrome = check_syndrome ();
        if (syndrome != 0)
        {
            st = correct_errors_erasures (encoded + in_index, en_chunk, 0, NULL);
            if (st != 1)
            {
                check_pass = false;
            }
        }
        memcpy (encoded + out_index, encoded + in_index, en_chunk - NPAR);
        out_index += en_chunk - NPAR; // don't keep parity bits in the output
        in_index += en_chunk;
        left -= en_chunk;
        i++;
    }

    return check_pass ? out_index : -1;
}


extern "C"
JNIEXPORT jcharArray Java_edu_gmu_cs_CirclsClient_RxHandler_ImageProcessor(JNIEnv &env, jobject, Mat &matRGB) {
    int pixels = matRGB.rows;
    int32_t frame[pixels][3];

    // flatten frame in Lab color-space
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);
    flattenCols(matLab, frame);
    matLab.release();

    // detect symbols

    // RS decoding
//    decod_rs();

    jcharArray ret = env.NewCharArray(pixels);
    if (ret != NULL) {
        jchar buf[pixels];

        // convert flat frame to symbols
        detectSymbols(frame, buf, pixels);

        // copy results to return
        env.SetCharArrayRegion(ret, 0, pixels, buf);
    }
    return ret;
}

#define PULSE_WIDTH 8
#define IR_PACKET_SIZE 20

extern "C"
JNIEXPORT jintArray Java_edu_gmu_cs_CirclsClient_TxHandler_GetNAKPattern(JNIEnv &env, jobject, jint id) {
    // magic + fcs
    jint buf[IR_PACKET_SIZE] = {1 * PULSE_WIDTH,
                                1 * PULSE_WIDTH,
                                1 * PULSE_WIDTH,
                                5 * PULSE_WIDTH};

    // each bit is represented by a total of 3 pulses
    int i = 4;
    for (int b = 7; b >= 0; b--) {
        bool set = (id >> b) & 1;
        buf[i++] = (set ? 2 : 1) * PULSE_WIDTH; // on
        buf[i++] = (set ? 1 : 2) * PULSE_WIDTH; // off
    }

    jintArray ret = env.NewIntArray(IR_PACKET_SIZE);
    if (ret != NULL) {
        env.SetIntArrayRegion(ret, 0, IR_PACKET_SIZE, buf);
    }
    return ret;
}
