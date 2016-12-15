#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
extern "C"
{
    #include "rscode-1.3/ecc.h"
}

using namespace std;
using namespace cv;

#define PULSE_WIDTH 8
#define IR_PACKET_SIZE 32
#define BITS_PER_SYMBOL 2
#define MAX_ID 256

// takes an OpenCV Matrix of 3D pixels
// returns a single averaged column of 3D pixels
void flattenCols(Mat &mat, int32_t flat[][3])
{
    // get Mat properties
    int rows = mat.rows;
    int cols = mat.cols;
    uint8_t *data = (uint8_t *)mat.data;

    // initialize flat array
    memset(flat, 0, sizeof(int32_t) * rows);

    for (int i = 0; i < rows; i++)
    {
        // sum column values across the row
        for (int j = 0; j < cols; j++)
        {
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
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols; j++)
        {
            flat[j][0] += (*data++);
            flat[j][1] += (*data++);
            flat[j][2] += (*data++);
        }
    }

    // average the values for each flat column
    for (int i = 0; i < cols; i++)
    {
        flat[i][0] /= rows;
        flat[i][1] = flat[i][1] / rows - 128;
        flat[i][2] = flat[i][2] / rows - 128;
    }
}

// takes a flat frame of pixels, symbol storage, and the number of pixels
// returns the symbols detected and # of symbols
int detectSymbols(int32_t frame[][3], uint8_t symbols[], int pixels)
{
    char p = '0';
    int len = 0;

    // convert Lab numbers to 01RGBY representation
    for (int i = 0; i < pixels; i++)
    {
        int L = frame[i][0];
        int a = frame[i][1] - 128;
        int b = frame[i][2] - 128;
        char c;

        // +L = white, +a = red; -a = green; -b = blue; +b = yellow;
        if (a == b)
        {
            c = (L == 0) ? '0' : '1';
        }
        else if (abs(a) > abs(b))
        {
            c = (a < 0) ? 'G' : 'R';
        }
        else
        {
            c = (b < 0) ? 'B' : 'Y';
        }

        // same as last pixel?
        if (c != p)
        {
            symbols[len++] = p;
            p = c;
        }
    }

    return len;
}


// convert symbols into bits and return as bytes
int demodulate(uint8_t data[], int len)
{
    int s = 0;

    for (int i = 0; i < len; i++)
    {
        uint8_t n = 0;
        for (int j = 0; j < 8; j += 2)
        {
            uint8_t b;
            switch(data[s++])
            {
                case 'R':
                    b = 0x00;
                    break;
                case 'G':
                    b = 0x01;
                    break;
                case 'B':
                    b = 0x10;
                    break;
                case 'Y':
                    b = 0x11;
                    break;
                default:
                    continue;
            }

            n |= b << j;
        }
        data[i] = n;
    }
}


int decode_rs (uint8_t *encoded, size_t length)
{
    bool check_pass = true;
    int st;

    // Then decode the remainder of the message, in 255-byte chunks.
    // The input in 255 - NPAR byte data chunks, with NPAR parity bits after each
    // chunk, plus a possibly smaller chunk at the end, followed by a FCS.
    size_t left = length;
    size_t i = 0;
    size_t out_index = 0;
    size_t in_index = 0;
    while (left > 0)
    {
        size_t en_chunk = 255;
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
JNIEXPORT jcharArray Java_edu_gmu_cs_CirclsClient_RxHandler_FrameProcessor(JNIEnv &env, jobject,
                                                                           Mat &matRGB)
{
    int num_pixels = matRGB.rows;
    uint8_t data[num_pixels];

    // flatten frame in Lab color-space
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    // flatten frame in Lab color-space
    int32_t frame[num_pixels][3];
    flattenCols(matLab, frame);
    matLab.release();

    // detect symbols
    int num_symbols = detectSymbols(frame, data, num_pixels);
/*
    // demodulate symbols into bytes
    int num_encoded = demodulate(data, num_symbols);

    // RS decoding
    int num_bytes = decode_rs(data, num_encoded);
*/
    jcharArray ret = env.NewCharArray(num_symbols);
    if (ret != NULL) {
        jchar buf[num_symbols];

        buf[0] = num_symbols;
        for (int i = 1; i < num_symbols; i++)
        {
            buf[i] = data[i];
        }

        // copy results to return
        env.SetCharArrayRegion(ret, 0, num_symbols, buf);
    }
    return ret;
}


extern "C"
JNIEXPORT jintArray Java_edu_gmu_cs_CirclsClient_TxHandler_GetNAKPattern(JNIEnv &env, jobject,
                                                                         jint id)
{
    jint buf[IR_PACKET_SIZE];

    // magic + fcs
    id |= 0b10100000 << 8;

    // each bit is represented by a total of 4 pulses
    for (int i = 0, b = 15; b >= 0; b--)
    {
        bool set = (id >> b) & 1;
        buf[i++] = (set ? 3 : 1) * PULSE_WIDTH; // on
        buf[i++] = (set ? 1 : 3) * PULSE_WIDTH; // off
    }

    jintArray ret = env.NewIntArray(IR_PACKET_SIZE);
    if (ret != NULL)
    {
        env.SetIntArrayRegion(ret, 0, IR_PACKET_SIZE, buf);
    }
    return ret;
}
