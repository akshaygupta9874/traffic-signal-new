// JNI glue: Java_edu_trafficsignal_TrafficEngine_* -> traffic_c_api.  Build with the NDK
// (see CMakeLists.txt). Not compiled by the desktop Makefile because it needs <jni.h>.
#include <jni.h>
#include "../traffic_c_api.h"

#define H(h) reinterpret_cast<TrafficHandle*>(h)

extern "C" {
JNIEXPORT jlong JNICALL Java_edu_trafficsignal_TrafficEngine_nativeCreate(JNIEnv*, jobject, jdouble g, jdouble y, jdouble r) { return reinterpret_cast<jlong>(traffic_create(g, y, r)); }
JNIEXPORT void JNICALL Java_edu_trafficsignal_TrafficEngine_nativeDestroy(JNIEnv*, jobject, jlong h) { traffic_destroy(H(h)); }
JNIEXPORT void JNICALL Java_edu_trafficsignal_TrafficEngine_nativeUpdate(JNIEnv*, jobject, jlong h, jdouble dt) { traffic_update(H(h), dt); }
JNIEXPORT jboolean JNICALL Java_edu_trafficsignal_TrafficEngine_nativeSetState(JNIEnv*, jobject, jlong h, jint s) { return traffic_set_state(H(h), s) != 0; }
JNIEXPORT jboolean JNICALL Java_edu_trafficsignal_TrafficEngine_nativeSetDirection(JNIEnv*, jobject, jlong h, jint m) { return traffic_set_direction(H(h), m) != 0; }
JNIEXPORT jboolean JNICALL Java_edu_trafficsignal_TrafficEngine_nativeSetDuration(JNIEnv*, jobject, jlong h, jint s, jdouble sec) { return traffic_set_duration(H(h), s, sec) != 0; }
JNIEXPORT void JNICALL Java_edu_trafficsignal_TrafficEngine_nativeAllOff(JNIEnv*, jobject, jlong h) { traffic_all_off(H(h)); }
JNIEXPORT void JNICALL Java_edu_trafficsignal_TrafficEngine_nativeReset(JNIEnv*, jobject, jlong h) { traffic_reset(H(h)); }
JNIEXPORT void JNICALL Java_edu_trafficsignal_TrafficEngine_nativeSetPaused(JNIEnv*, jobject, jlong h, jboolean p) { traffic_set_paused(H(h), p); }
JNIEXPORT jint JNICALL Java_edu_trafficsignal_TrafficEngine_nativeGetState(JNIEnv*, jobject, jlong h) { return traffic_get_state(H(h)); }
JNIEXPORT jint JNICALL Java_edu_trafficsignal_TrafficEngine_nativeGetDirection(JNIEnv*, jobject, jlong h) { return traffic_get_direction(H(h)); }
JNIEXPORT jdouble JNICALL Java_edu_trafficsignal_TrafficEngine_nativeGetRemaining(JNIEnv*, jobject, jlong h) { return traffic_get_remaining(H(h)); }
JNIEXPORT jdouble JNICALL Java_edu_trafficsignal_TrafficEngine_nativeGetDuration(JNIEnv*, jobject, jlong h) { return traffic_get_duration(H(h)); }
}
