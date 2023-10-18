#include <jni.h>
#include <android/log.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iomanip>
#include <sstream>

#define LOG_TAG    "native-lib"
#define ALOG(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)

#define DEBUG // avoid assert FindErrors
#include "rs.hpp"
#define NMSG 13
#define NPAR 4
RS::ReedSolomon<NMSG, NPAR> rs;

using namespace std;
using namespace cv;

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    return JNI_VERSION_1_6;
}


// takes an OpenCV Matrix of 3D pixels
// returns a single averaged row of 3D pixels
void flattenMatrix(Mat &mat, int32_t flat[][3]) {
    // get Mat properties
    int rows = mat.rows;
    int cols = mat.cols;
    auto *data = (uint8_t *)mat.data;

    // initialize flat frame
    memset(flat, 0, sizeof(int32_t) * cols * 3);

    // for each row
    for (int i = 0; i < rows; i++)
    {
        // sum up each col
        for (int j = 0; j < cols; j++)
        {
            flat[cols - j - 1][0] += (*data++);
            flat[cols - j - 1][1] += (*data++);
            flat[cols - j - 1][2] += (*data++);
        }
    }

    // calculate col averages and adjust
    for (int j = 0; j < cols; j++)
    {
        flat[j][0] /= rows;
        flat[j][1] = flat[j][1] / rows - 128;
        flat[j][2] = flat[j][2] / rows - 128;
    }
}


// takes symbol storage, a flat frame of pixels, and the number of pixels
int detectSymbols( uint8_t symbols[][2], int32_t frame[][3], int pixels )
{
    std::stringstream ss;
    int count = 0;          // symbol index
    int width = 0;
    char p = ' ';           // previous symbol

    // convert Lab numbers to 01RGBY representation
    for (int i = 0; i < pixels; i++)
    {
        int L = frame[i][0];
        int a = frame[i][1];
        int b = frame[i][2];
        char c;

        // +L = white, +a = red; -a = green; -b = blue; +b = yellow;
        if (L < 20) {
            c = '0';
        } else if (abs(a) < 30 && abs(b) < 30) {
            c = '1';
        } else {
            if (abs(a) > abs(b)) {
                c = (a < 0) ? 'G' : 'R';
            } else {
                c = (b < 0) ? 'B' : 'Y';
            }
        }

        ss << '(' << frame[i][0] << ',' << frame[i][1] << ',' << frame[i][2] << ' ' << c << ')';

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

    ALOG("Frame: %s", ss.str().c_str());
    return count;
}


// convert symbols into bits and return as bytes
int demodulate(uint8_t data[], int dataLen, uint8_t symbols[][2], int symbolLen)
{
    int i = 7;         // symbol index
    int j = 0;         // data index
    int k = 0;         // bit index
    int width;

    // look for sync sequence
    for (; i < symbolLen; i++)
    {
        if (symbols[i - 7][0] == '1' && symbols[i - 6][0] == '0' && symbols[i - 5][0] == '1' && symbols[i - 4][0] == '0' && symbols[i - 3][0] == '1' && symbols[i - 2][0] == '0' && symbols[i - 1][0] == '1' && symbols[i][0] == '0') {
            width = (symbols[i - 7][1] + symbols[i - 5][1] + symbols[i - 3][1] + symbols[i - 1][1]) * 3 / 16;
            ALOG("Symbol Width: %d", width);
            break;
        }
    }

    // process remaining symbols
    while (i < symbolLen && j < dataLen)
    {
        uint8_t b = 0;
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
            // new byte
            if (k == 0) {
                data[j] = 0;
            }

            // insert data symbol
            data[j] |= b << k;

            // update bit index
            k = (k + 2) % 8;

            // next byte?
            if (k == 0) {
                j++;
            }

            symbols[i][1] -= width;
        } else {
            // ignore symbols that are too small
            i++;
        }
    }

    return j;
}


extern "C"
JNIEXPORT jcharArray JNICALL Java_edu_gmu_cs_CirclsClient_RxHandler_FrameProcessor(JNIEnv &env, jobject obj,
                                                                           jint width, jint height, jobject pixels) {
    uint8_t data[NMSG+NPAR];
    int num_decoded = 0;

    if (width > 0 && height > 0) {
        // build matrix around RGBA frame
        Mat matRGB(height, width, CV_8UC4, env.GetDirectBufferAddress(pixels));

        // convert frame to Lab color-space
        Mat matLab;
        cvtColor(matRGB, matLab, COLOR_RGB2Lab);
        matRGB.release();

        // flatten frame
        int num_pixels = width;
        int32_t frame[num_pixels][3];
        flattenMatrix(matLab, frame);
        matLab.release();

        // detect symbols
        uint8_t symbols[num_pixels][2];
        int num_symbols = detectSymbols(symbols, frame, num_pixels);

        // demodulate
        int num_encoded = demodulate(data, NMSG+NPAR, symbols, num_symbols);

        // decode if we have a full message
        if (num_encoded == NMSG+NPAR) {
            num_decoded = rs.Decode(data, data) ? 0 : NMSG;
        }
        ALOG("Encoded: %d, Decoded: %d, Id: %d, Message: %.*s %x %x %x %x",
             num_encoded, num_decoded,
             data[0], NMSG - 1, (data + 1),
             data[num_encoded - 3], data[num_encoded - 2], data[num_encoded - 1], data[num_encoded]);
    }

    // return text
    jcharArray message = env.NewCharArray(num_decoded);
    if (message != nullptr) {
        jchar buf[num_decoded];
        for (int i = 0; i < num_decoded; i++) {
            buf[i] = data[i];
        }
        env.SetCharArrayRegion(message, 0, num_decoded, buf);
    }

    return message;
}
