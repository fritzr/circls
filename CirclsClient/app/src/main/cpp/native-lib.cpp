#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
extern "C"
{
    #include "rscode-1.3/ecc.h"
}

#define  LOG_TAG    "native-lib"
#define  ALOG(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

using namespace std;
using namespace cv;

// reference to message receiver
static jmethodID midStr = NULL;

// hold symbols captured across multiple frames
jint id = 0;

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
// puts into symbol_buffer and updates last
int detectSymbols( uint8_t symbols[], int32_t frame[][3], int pixels )
{
    int j = 0; // symbol index
    char p = '0';
    int width = 0; // current symbol width

    // convert Lab numbers to 01RGBY representation
    for (int i = 0; i < pixels; i++)
    {
        int L = frame[i][0];
        int a = frame[i][1];
        int b = frame[i][2];
        char c;

        // +L = white, +a = red; -a = green; -b = blue; +b = yellow;
        if (a == b || L < 40)
        {
            continue;
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
        if (c != p || width > i/(j+1))
        {
            symbols[j++] = p;
            p = c;
            width = 0;
        } else {
            width++;
        }
    }

    return j;
}


// convert symbols into bits and return as bytes
int demodulate(uint32_t data[], uint8_t symbols[], int len)
{
    int j = 0;     // data index
    int k = 0;     // bit index
    data[j] = 0;

    // process all symbols
    for (int i = 0; i < len; i++)
    {
        uint32_t b = 0;
        switch (symbols[i]) {
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
                // non-data symbol
                continue;
        }

        // insert data symbol
        data[j] |= b << k;

        // update bit index
        k = (k + 2) % 32;

        // start next word
        if (k == 0) {
            j++;
            data[j] = 0;
        }
    }

    // number of words demodulated
    return j;
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
JNIEXPORT jcharArray Java_edu_gmu_cs_CirclsClient_RxHandler_FrameProcessor(JNIEnv &env, jobject obj,
                                                                           Mat &matRGB) {
    int num_pixels = matRGB.rows;

    // flatten frame in Lab color-space
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    // flatten frame in Lab color-space
    int32_t frame[num_pixels][3];
    flattenCols(matLab, frame);
    matLab.release();

    // detect symbols
    uint8_t symbols[num_pixels];
    int num_symbols = detectSymbols(symbols, frame, num_pixels);

/*
    // return symbols
    jcharArray message = env.NewCharArray(num_symbols);
    if (message != NULL) {
        jchar buf[num_symbols];
        for (int i = 0; i < num_symbols; i++) {
            buf[i] = symbols[i];
        }
        env.SetCharArrayRegion(message, 0, num_symbols, buf);
    }
*/

    // demodulate
    uint8_t data[(num_symbols / 4) + 1]; // upper bound # bytes
    int num_encoded = 4 * demodulate((uint32_t*) &data[0], symbols, num_symbols);

    // return text
    jcharArray message = env.NewCharArray(num_encoded);
    if (message != NULL) {
        jchar buf[num_encoded];
        for (int i = 0; i < num_encoded; i++) {
            buf[i] = data[i];
        }
        env.SetCharArrayRegion(message, 0, num_encoded, buf);
    }

    return message;
}
