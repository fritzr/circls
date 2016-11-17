#include <jni.h>
#include <opencv2/core/core.hpp>

using namespace std;
using namespace cv;

extern "C"
jintArray Java_edu_gmu_cs_CirclsClient_MainActivity_ImageProcessor(JNIEnv *env, jobject, Mat &mat) {
    jint rgb[] = {0, 0, 0};

    unsigned long pixels = mat.total();
    uchar *data = mat.data;
    for (long i = 0; i < pixels; i++) {
        rgb[0] += (*data++);
        rgb[1] += (*data++);
        rgb[2] += (*data++);
        data++;
    }

    rgb[0] /= pixels;
    rgb[1] /= pixels;
    rgb[2] /= pixels;

    jintArray ret = env->NewIntArray(3);
    if (ret != NULL) {
        env->SetIntArrayRegion(ret, 0, 3, rgb);
    }
    return ret;
}
