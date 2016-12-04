#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>


using namespace std;
using namespace cv;

extern "C"
jintArray Java_edu_gmu_cs_CirclsClient_MainActivity_ImageProcessor(JNIEnv &env, jobject, Mat &matRGB) {
    Mat matLab;
    cvtColor(matRGB, matLab, CV_RGB2Lab);

    int rows = matLab.rows;
    int cols = matLab.cols;

    jint flat[cols][2];
    memset(flat, 0, sizeof(flat));

    // calculate total ab values for each column
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

    jintArray ret = env.NewIntArray(cols * 2);
    if (ret != NULL) {
        env.SetIntArrayRegion(ret, 0, cols * 2, (jint*) flat);
    }
    return ret;
}
