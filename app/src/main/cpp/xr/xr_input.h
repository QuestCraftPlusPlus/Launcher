//
// Created by CADIndie on 12/13/2024.
//

#ifndef QCXR_XR_INPUT_H
#define QCXR_XR_INPUT_H
#include "xr_include.h"
#include "xr_linear_algebra.h"
#include <stdbool.h>


typedef struct {
    XrPosef handPose[2];
    float worldRotationY;
} xr_input_t;

extern xr_input_t xrInput;

bool pollActions(XrTime predictedTime);
bool attachActionSet();
bool createActionSet();
bool createDefaultActions();
bool createSuggestedBindings();
void createActionPoses();

void getControllerRay(int controller, XrMatrix4x4f model, XrVector3f* startOut, XrVector3f* endOut);
bool controllerRayIntersectsScreen(int controllerIndex, float* u, float* v);
bool rayIntersectsTriangle(XrVector3f origin, XrVector3f direction, XrVector3f* tri, float* u, float* v, float* t);

#endif //QCXR_XR_INPUT_H