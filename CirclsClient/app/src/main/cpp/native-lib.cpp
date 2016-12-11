#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "rs"

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
    int32_t flat[cols][2];
    memset(flat, 0, sizeof(flat));

    // total ab values for each row
    uint8_t *data = (uint8_t *)matLab.data;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            data++; // skip L
            flat[j][0] += (*data++);
            flat[j][1] += (*data++);
        }
    }

    // average the ab values for each column
    for (int i = 0; i < cols; i++) {
        flat[i][0] /= rows;
        flat[i][1] /= rows;
    }

    jcharArray ret = env.NewCharArray(cols);
    if (ret != NULL) {
        jchar buf[cols];

        // convert ab numbers to RGBYW representation
        for (int i = 0; i < cols; i++) {
            int a = flat[i][0] - 128;
            int b = flat[i][1] - 128;
            jchar c;

            // +a = red; -a = green; -b = blue; +b = yellow;
            if (a == b) {
                c = 'w';
            }
            else if (abs(a) > abs(b)) {
                c = (a < 0) ? 'G' : 'R';
            }
            else {
                c = (b < 0) ? 'B' : 'Y';
            }

            buf[i] = c;
        }

        // copy results to return
        env.SetCharArrayRegion(ret, 0, cols, buf);
    }
    return ret;
}
*/

extern "C"
jcharArray Java_edu_gmu_cs_CirclsClient_RxWorker_ImageProcessor(JNIEnv &env, jobject, Mat &matRGB) {
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    int rows = matLab.rows;
    int cols = matLab.cols;

    // initialize flat frame
    int32_t flat[rows][2];
    memset(flat, 0, sizeof(flat));

    // total ab values for each row
    uint8_t *data = (uint8_t *)matLab.data;
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
            int a = flat[i][0] - 128;
            int b = flat[i][1] - 128;
            jchar c;

            // +a = red; -a = green; -b = blue; +b = yellow;
            if (a == b) {
                c = 'w';
            }
            else if (abs(a) > abs(b)) {
                c = (a < 0) ? 'G' : 'R';
            }
            else {
                c = (b < 0) ? 'B' : 'Y';
            }

            buf[i] = c;
        }

        // copy results to return
        env.SetCharArrayRegion(ret, 0, rows, buf);
    }
    return ret;
}
