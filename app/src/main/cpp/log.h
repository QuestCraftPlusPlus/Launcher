//
// Created by maks on 03.12.2024.
//

#ifndef QCXR_LOG_H
#define QCXR_LOG_H

#include <android/log.h>
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#endif //QCXR_LOG_H
