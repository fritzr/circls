#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
extern "C"
{
    #include "rscode-1.3/ecc.h"
}

using namespace std;
using namespace cv;

// reference to message receiver
static jmethodID midStr = NULL;

#define BUFFER_SIZE sizeof(symbol_buffer)

// hold symbols captured across multiple frames
static char header_pattern[] = "01010RGBY";
static char trailer_pattern[] = "010";
static uint8_t symbol_buffer[1024 * 256];
static uint32_t first = 0;
static uint32_t last = 0;


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
void detectSymbols(int32_t frame[][3], int pixels)
{
    uint32_t j = last; // symbol index
    char p = '0';

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
            symbol_buffer[j] = p;
            j = (j + 1) % BUFFER_SIZE;
            p = c;
        }
    }

    // update symbol_buffer last index
    last = j;
}


// convert symbols into bits and return as bytes
int demodulate(uint8_t data[], int len)
{
    uint32_t i = first; // symbol index
    int j = 0;     // data index
    int k = 0;     // bit index

    // process all symbols starting at first up to len
    for (; i != (first + len - 1) %  BUFFER_SIZE; i = (i + 1) %  BUFFER_SIZE) {
        uint8_t b;
        switch(symbol_buffer[i])
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
                // non-data symbol
                continue;
        }

        // insert data symbol
        data[j] |= b << k;

        // update bit index
        k = (k + 2) % 8;

        // update data index
        if (k == 0)
            j++;
    }

    // update symbol_buffer first index
    first = i;

    // actual number of bytes demodulated
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


// search for header followed by arbitrary symbols and a trailer
int findPacket()
{
    bool found_header = false;
    int pos = 0;
    int i = first;

    for (; i != (last - 1 + BUFFER_SIZE) %  BUFFER_SIZE; i = (i + 1) %  BUFFER_SIZE) {
        if (symbol_buffer[i] == header_pattern[pos]) {
            pos++;
        } else {
            pos=0;
        }
        if (pos == strlen(header_pattern)) {
            // can't really do anything with any prior symbols
            first = i;

            // found it
            found_header = true;
            break;
        }
    }

    // if we have a header, let's look for trailer
    if (found_header) {
        pos = 0;
        for (; i != last; i = (i + 1) % BUFFER_SIZE) {
            if (symbol_buffer[i] == trailer_pattern[pos]) {
                pos++;
            } else {
                pos = 0;
            }
            if (pos == strlen(trailer_pattern)) {
                // we have a packet
                return i;
            }
        }
    }

    return 0;
}

extern "C"
JNIEXPORT void Java_edu_gmu_cs_CirclsClient_RxHandler_FrameProcessor(JNIEnv &env, jobject obj,
                                                                           Mat &matRGB)
{
    // only need to look this up once
    if (midStr == NULL ) {
        midStr = env.GetMethodID(env.GetObjectClass(obj), "receive", "(I[C)V");
    }

    int num_pixels = matRGB.rows;

    // flatten frame in Lab color-space
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    // flatten frame in Lab color-space
    int32_t frame[num_pixels][3];
    flattenCols(matLab, frame);
    matLab.release();

    // detect symbols
    detectSymbols(frame, num_pixels);

    // this will display the raw symbols and get out
/*
    jcharArray message = env.NewCharArray(last);
    if (message != NULL) {
        jchar buf[last];

        for (int i = 0; i < last; i++)
        {
            buf[i] = symbol_buffer[i];
        }

        // skip the first byte
        env.SetCharArrayRegion(message, first, last, buf);

        // send a complete message back
        env.CallVoidMethod(obj, midStr, last, message);
    }
    first = 0;
    last = 0;
    return;
*/

    // search for packets
    int num_symbols;
    while (num_symbols = findPacket()) {

// debug stuff
/*
        jcharArray test = env.NewCharArray(6);
        if (test != NULL) {
            jchar buf[] = { 'p', 'a', 'c', 'k', 'e', 't'};
            env.SetCharArrayRegion(test, 0, 6, buf);
            env.CallVoidMethod(obj, midStr, num_symbols, test);
        }
*/
        uint8_t data[num_symbols];
        int num_encoded = demodulate(data, num_symbols);

// debug stuff
/*
        test = env.NewCharArray(5);
        if (test != NULL) {
            jchar buf[] = { 'd', 'e', 'm', 'o', 'd'};
            env.SetCharArrayRegion(test, 0, 5, buf);
            env.CallVoidMethod(obj, midStr, num_encoded, test);
        }
*/

        // RS decoding
        int num_bytes = decode_rs(data, num_encoded);

        // if it decoded successfully, we have a message!
        if (num_bytes) {
            // copy data to Java char array
            jcharArray message = env.NewCharArray(num_bytes);
            if (message != NULL) {
                jchar buf[num_bytes];

                for (int i = 0; i < num_bytes - 1; i++) {
                    buf[i] = data[i + 1];
                }

                // first byte should be the id
                env.SetCharArrayRegion(message, 0, num_bytes - 1, buf);

                // send a complete message back
                env.CallVoidMethod(obj, midStr, data[0], message);
            }
        }

    }
}
