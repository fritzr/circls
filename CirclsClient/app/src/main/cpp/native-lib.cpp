#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>


using namespace std;
using namespace cv;
/*
extern "C"
jcharArray Java_edu_gmu_cs_CirclsClient_MainActivity_ImageProcessor(JNIEnv &env, jobject, Mat &matRGB) {
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    int rows = matLab.rows;
    int cols = matLab.cols;

    // initialize flat frame
    int flat[cols][2];
    memset(flat, 0, sizeof(flat));

    // total ab values for each column
    uchar *data = matLab.data;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            data++; // skip L
            flat[j][0] += (*data++);
            flat[j][1] += (*data++);
        }
    }

    // average the ab values for each column
    for (int j = 0; j < cols; j++) {
        flat[j][0] /= rows;
        flat[j][1] /= rows;
    }

    jcharArray ret = env.NewCharArray(rows);
    if (ret != NULL) {
        jchar buf[rows];

        // convert ab numbers to RGBYW representation
        for (int j = 0; j < cols; j++) {
            jchar c;

            // shift ab values to +/- and truncate less significant bits
            int a = (flat[j][0] - 128) & (0xf8);
            int b = (flat[j][1] - 128) & (0xf8);

            // +a = red; -a = green; -b = blue; +b = yellow;
            if (a == b) {
                c = 'w';
            }
            else if ((a & 0x7f) > (b & 0x7f)) {
                c = (a & 0x80) ? 'G' : 'R';
            }
            else {
                c = (b & 0x80) ? 'B' : 'Y';
            }

            buf[j] = c;
        }

        // copy results to return
        env.SetCharArrayRegion(ret, 0, rows, buf);
    }
    return ret;
}
*/

extern "C"
jcharArray Java_edu_gmu_cs_CirclsClient_MainActivity_ImageProcessor(JNIEnv &env, jobject, Mat &matRGB) {
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    int rows = matLab.rows;
    int cols = matLab.cols;

    // initialize flat frame
    int flat[rows][2];
    memset(flat, 0, sizeof(flat));

    // total ab values for each row
    uchar *data = matLab.data;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            data++; // skip L
            flat[i][0] += (*data++);
            flat[i][1] += (*data++);
        }

        // average the ab values
        flat[i][0] /= cols;
        flat[i][1] /= cols;
    }

    jcharArray ret = env.NewCharArray(rows);
    if (ret != NULL) {
        jchar buf[rows];

        // convert ab numbers to RGBYW representation
        for (int i = 0; i < rows; i++) {
            jchar c;

            // shift ab values to +/- and truncate less significant bits
            int a = (flat[i][0] - 128) & (0xf8);
            int b = (flat[i][1] - 128) & (0xf8);

            // +a = red; -a = green; -b = blue; +b = yellow;
            if (a == b) {
                c = 'w';
            }
            else if ((a & 0x7f) > (b & 0x7f)) {
                c = (a & 0x80) ? 'G' : 'R';
            }
            else {
                c = (b & 0x80) ? 'B' : 'Y';
            }

            buf[i] = c;
        }

        // copy results to return
        env.SetCharArrayRegion(ret, 0, rows, buf);
    }
    return ret;
}
