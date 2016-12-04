#include <jni.h>
#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>


using namespace std;
using namespace cv;

extern "C"
jintArray Java_edu_gmu_cs_CirclsClient_MainActivity_ImageProcessor(JNIEnv &env, jobject, Mat &input) {
    jint lab[] = {0, 0, 0};

    Mat output;
    cvtColor(input, output, CV_RGB2Lab);

    unsigned long pixels = output.total();
    uchar *data = output.data;
    for (long i = 0; i < pixels; i++) {
        lab[0] += (*data++);
        lab[1] += (*data++);
        lab[2] += (*data++);
    }

    lab[0] /= pixels;
    lab[1] /= pixels;
    lab[2] /= pixels;

    jintArray ret = env.NewIntArray(3);
    if (ret != NULL) {
        env.SetIntArrayRegion(ret, 0, 3, lab);    }
    return ret;
}
