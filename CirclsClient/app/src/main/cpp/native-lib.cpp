#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iomanip>
#include <sstream>
extern "C"
{
    #include "rscode-1.3/ecc.h"
}

#define LOG_TAG    "native-lib"
#define ALOG(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

using namespace std;
using namespace cv;


jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    // setup RS
    initialize_ecc();

    return JNI_VERSION_1_6;
}


// takes an OpenCV Matrix of 3D pixels
// returns a single averaged column of 3D pixels
void flattenCols(Mat &mat, int32_t flat[][3])
{
    // get Mat properties
    int rows = mat.rows;
    int cols = mat.cols;
    uint8_t *data = (uint8_t *)mat.data;

    // initialize flat frame
    memset(flat, 0, sizeof(int32_t) * rows * 3);

    // for each row
    for (int i = 0; i < rows; i++)
    {
        // sum up the entire row
        for (int j = 0; j < cols; j++)
        {
            flat[i][0] += (*data++);
            flat[i][1] += (*data++);
            flat[i][2] += (*data++);
        }

        // calculate row average and adjust
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
    memset(flat, 0, sizeof(int32_t) * cols * 3);

    // for each row
    for (int i = 0; i < rows; i++)
    {
        // sum up each col
        for (int j = 0; j < cols; j++)
        {
            flat[j][0] += (*data++);
            flat[j][1] += (*data++);
            flat[j][2] += (*data++);
        }
    }

    // calculate col averages and adjust
    for (int i = 0; i < cols; i++)
    {
        flat[i][0] /= rows;
        flat[i][1] = flat[i][1] / rows - 128;
        flat[i][2] = flat[i][2] / rows - 128;
    }
}


// takes symbol storage, a flat frame of pixels, and the number of pixels
int detectSymbols( uint8_t symbols[][2], int32_t frame[][3], int pixels )
{
    int count = 0;          // symbol index
    int width = 0;
    char p = '1';           // previous symbol

    // convert Lab numbers to 01RGBY representation
    for (int i = pixels - 1; i >= 0; i--)
    {
        int L = frame[i][0];
        int a = frame[i][1];
        int b = frame[i][2];
        char c;

        // +L = white, +a = red; -a = green; -b = blue; +b = yellow;

        if (abs(a) < 15 && abs(b) < 15) {
            c = (L < 15) ? '0' : '1';
        }
        if (abs(a) > abs(b))
        {
            c = (L < 15) ? '0' : (a < 0) ? 'G' : 'R';
        }
        else
        {
            c = (L < 15) ? '0' : (b < 0) ? 'B' : 'Y';
        }

        // same as last pixel?
        if (c != p)
        {
            // store previous symbol
            symbols[count][0] = p;
            symbols[count][1] = width;
            count++;

            // start tracking new symbol
            p = c;
            width = 1;
        } else {
            width++;
        }
    }

    return count;
}


// convert symbols into bits and return as bytes
int demodulate(uint8_t data[], uint8_t symbols[][2], int len)
{
    std::stringstream ss;
    int i = 7;         // symbol index
    int j = 0;         // data index
    int k = 0;         // bit index
    int width;

    // look for sync sequence
    for (; i < len; i++)
    {
        if (symbols[i - 7][0] == 'Y' && symbols[i - 6][0] == '0' && symbols[i - 5][0] == 'Y' && symbols[i - 4][0] == '0' && symbols[i - 3][0] == 'Y' && symbols[i - 2][0] == '0' && symbols[i - 1][0] == 'Y' && symbols[i][0] == '0') {
            width = (symbols[i - 7][1] + symbols[i - 5][1] + symbols[i - 3][1] + symbols[i - 1][1]) * 3 / 16;
            ALOG("Symbol Width: %d", width);
            break;
        }
    }

    // access data as words to preserve byte ordering
    uint32_t *words = (uint32_t *)data;
    words[j] = 0;

    // process remaining symbols
    while (i < len)
    {
        uint32_t b = 0;
        switch (symbols[i][0]) {
            case 'R':
                b = 0b00;
                break;
            case 'G':
                b = 0b01;
                break;
            case 'B':
                b = 0b10;
                break;
            case 'Y':
                b = 0b11;
                break;
            default:
                // non-data symbol
                i++;
                continue;
        }

        if (symbols[i][1] >= width) {
            // debugging
            ss << (char) symbols[i][0];

            // insert data symbol
            words[j] |= b << k;

            // update bit index
            k = (k + 2) % 32;

            // start of a new word?
            if (k == 0) {
                words[++j] = 0;
            }

            symbols[i][1] -= width;
        } else {
            // ignore symbols that are too small
            i++;
        }
    }

    ALOG("Used symbols: %s", ss.str().c_str());

    // number of bytes demodulated
    return j * 4 + k / 8;
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

    return check_pass ? out_index : 0;
}


extern "C"
JNIEXPORT jcharArray Java_edu_gmu_cs_CirclsClient_RxHandler_FrameProcessor(JNIEnv &env, jobject obj,
                                                                           jint width, jint height, jobject pixels) {
    int num_pixels = height;

    // copy flipped RGBA frame into matrix
    Mat matRGB(height, width, CV_8UC4);
    memcpy(matRGB.data, env.GetDirectBufferAddress(pixels), width * height * 4);

    // convert frame to Lab color-space
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);
    matRGB.release();

    // flatten frame
    int32_t frame[num_pixels][3];
    flattenCols(matLab, frame);
    matLab.release();

    std::stringstream ss;
    for (int i = 0; i < num_pixels; i++) ss << '(' << frame[i][0] << ',' << frame[i][1] << ',' << frame[i][2] <<')';
    ALOG("Frame: %s", ss.str().c_str());

    // detect symbols
    uint8_t symbols[num_pixels][2];
    int num_symbols = detectSymbols(symbols, frame, num_pixels);

    ss.str("");
    for (int i = 0; i < num_symbols; i++) ss << setw(3) << (char) symbols[i][0];
    ALOG("Symbols: %s", ss.str().c_str());
    ss.str("");
    for (int i = 0; i < num_symbols; i++) ss << setw(3) << (int) symbols[i][1];
    ALOG("Widths : %s", ss.str().c_str());

    // demodulate
    uint8_t data[256]; // upper bound # bytes
    int num_demodulated = demodulate(data, symbols, num_symbols);
    int num_encoded = data[0];

    // decode RS
    int num_decoded = decode_rs((data + 1), num_encoded);
    ALOG("Demodulated: %d Encoded: %d, Decoded: %d, Message: %.*s %x %x %x %x", num_demodulated, num_encoded, num_decoded, num_encoded - NPAR, (data + 1), data[num_encoded - 3], data[num_encoded - 2], data[num_encoded - 1], data[num_encoded]);

    // return text
    jcharArray message = env.NewCharArray(num_decoded);
    if (message != NULL) {
        jchar buf[num_decoded];
        for (int i = 0; i < num_decoded; i++) {
            buf[i] = data[i + 1];
        }
        env.SetCharArrayRegion(message, 0, num_decoded, buf);
    }

    return message;
}
