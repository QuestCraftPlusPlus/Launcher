//
// Created by CADIndie on 1/29/2026.
//

#include <jni.h>

#ifndef QUESTCRAFT_MAIN_H
#define QUESTCRAFT_MAIN_H

extern JNIEnv *globalEnv;
extern jclass xrActivityInputClass;
extern jmethodID XRActivityInput_clickScreenAtPosition;

void clickScreenAtPosition(JNIEnv *env, int x, int y);

#endif //QUESTCRAFT_MAIN_H