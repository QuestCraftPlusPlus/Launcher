//
// Created by CADIndie on 1/29/2026.
//

#include <jni.h>

#ifndef QUESTCRAFT_MAIN_H
#define QUESTCRAFT_MAIN_H

extern JavaVM *globalVm;
extern jclass xrActivityInputClass;
extern jmethodID XRActivityInput_clickScreenAtPosition;

static inline JNIEnv* getJniEnv() {
    JNIEnv *env;
    jint res = (*globalVm)->GetEnv(globalVm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        (*globalVm)->AttachCurrentThread(globalVm, &env, NULL);
    }
    return env;
}

void setVulkanSurface(JNIEnv *env, jobject surface);
void clickScreenAtPosition(JNIEnv *env, int x, int y);

#endif //QUESTCRAFT_MAIN_H