//
// Created by CADIndie on 12/13/2024.
//

#include "xr_input.h"
#include "xr_include.h"
#include "xr_init.h"
#include <string.h>
#include <stdlib.h>

#define LOG_TAG __FILE_NAME__
#include "log.h"
#include "main.h"
#include "renderer_types.h"
#include "renderer.h"

xr_input_t xrInput;

static XrActionSet actionSet = XR_NULL_HANDLE;
static XrAction palmPoseAction = XR_NULL_HANDLE;
static XrAction vibrateAction = XR_NULL_HANDLE;
static XrAction clickScreenAction = XR_NULL_HANDLE;
static float vibrate[2] = {0, 0};
static XrPath handPaths[2];
static XrSpace handPoseSpace[2];
static XrActionStatePose handPoseState[2] = {{XR_TYPE_ACTION_STATE_POSE}, {XR_TYPE_ACTION_STATE_POSE}};
XrActionStateBoolean clickScreenState[2] = {{XR_TYPE_ACTION_STATE_BOOLEAN}, {XR_TYPE_ACTION_STATE_BOOLEAN}};

static XrVector3f screenTri1[3] =  {
        { 3, 8, 21.9f},
        {-5, 8, 21.9f},
        {-5, 4, 21.9f},
};
static XrVector3f screenTri2[3] =  {
        { 3, 8, 21.9f},
        {-5, 4, 21.9f},
        { 3, 4, 21.9f}
};

bool createActionSet() {
    XrActionSetCreateInfo actionSetCreateInfo = {XR_TYPE_ACTION_SET_CREATE_INFO};
    strncpy(actionSetCreateInfo.actionSetName, "main-action-set", XR_MAX_ACTION_SET_NAME_SIZE);
    strncpy(actionSetCreateInfo.localizedActionSetName, "Main Action Set", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    actionSetCreateInfo.priority = 0;
    XR_FAILRETURN(xrCreateActionSet(xrinfo.instance, &actionSetCreateInfo, &actionSet), false);
    return true;
}

bool createAction(XrAction* xrAction, char name[], char localizedName[], XrActionType xrActionType, size_t subaction_count) {
    XrActionCreateInfo actionCreateInfo = {XR_TYPE_ACTION_CREATE_INFO};
    actionCreateInfo.actionType = xrActionType;
    strncpy(actionCreateInfo.actionName, name, XR_MAX_ACTION_NAME_SIZE);
    strncpy(actionCreateInfo.localizedActionName, localizedName, XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    actionCreateInfo.countSubactionPaths = subaction_count;
    actionCreateInfo.subactionPaths = handPaths;
    XR_FAILRETURN(xrCreateAction(actionSet, &actionCreateInfo, xrAction), false);
    return true;
}

bool createDefaultActions() {
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/left", &handPaths[0]), false);
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/right", &handPaths[1]), false);
    createAction(&palmPoseAction, "palm-pose", "Palm Pose",XR_ACTION_TYPE_POSE_INPUT, 2);
    createAction(&vibrateAction, "vibrate", "Vibrate", XR_ACTION_TYPE_VIBRATION_OUTPUT, 2);
    createAction(&clickScreenAction, "clickscreen", "Click Screen", XR_ACTION_TYPE_BOOLEAN_INPUT, 2);
    return true;
}

bool suggestBindings(XrPath path, const XrActionSuggestedBinding bindings[], int binding_count) {
    XrInteractionProfileSuggestedBinding interactionProfileSuggestedBinding = {XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    interactionProfileSuggestedBinding.interactionProfile = path;
    interactionProfileSuggestedBinding.suggestedBindings = bindings;
    interactionProfileSuggestedBinding.countSuggestedBindings = binding_count;
    XR_FAILRETURN(xrSuggestInteractionProfileBindings(xrinfo.instance, &interactionProfileSuggestedBinding), false);
    return true;
}

bool createSuggestedBindings() {
    XrPath posePath[2];
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/left/input/grip/pose", &posePath[0]), false);
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/right/input/grip/pose", &posePath[1]), false);

    XrPath vibratePath[2];
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/left/output/haptic", &vibratePath[0]), false);
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/right/output/haptic", &vibratePath[1]), false);

    XrPath clickScreenPath[2];
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/left/input/trigger/value", &clickScreenPath[0]), false);
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/user/hand/right/input/trigger/value", &clickScreenPath[1]), false);

    const XrActionSuggestedBinding bindings[6] = {
            palmPoseAction, posePath[0],
            palmPoseAction, posePath[1],

            vibrateAction, vibratePath[0],
            vibrateAction, vibratePath[1],

            clickScreenAction, clickScreenPath[0],
            clickScreenAction, clickScreenPath[1]
    };

    XrPath controllerPath;
    XR_FAILRETURN(xrStringToPath(xrinfo.instance, "/interaction_profiles/oculus/touch_controller", &controllerPath), false);
    if (controllerPath == XR_NULL_PATH) {
        LOGE("Controller binding NULL: %s", __func__ );
    }
    suggestBindings(controllerPath, bindings, 6);
    return true;
}

bool createActionPoseSpace(XrAction xrAction, XrSpace* xrSpace, XrPath subactionPath) {
    const XrPosef xrPoseIdentity = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    XrActionSpaceCreateInfo actionSpaceCI = {XR_TYPE_ACTION_SPACE_CREATE_INFO};
    actionSpaceCI.action = xrAction;
    actionSpaceCI.poseInActionSpace = xrPoseIdentity;
    actionSpaceCI.subactionPath = subactionPath;

    XR_FAILRETURN(xrCreateActionSpace(xrinfo.session, &actionSpaceCI, xrSpace), false);
    return true;
}

void createActionPoses() {
    if (!createActionPoseSpace(palmPoseAction, &handPoseSpace[0], handPaths[0])) {
        LOGE("Failed to create hand pose space for left hand.");
    }

    if (!createActionPoseSpace(palmPoseAction,&handPoseSpace[1], handPaths[1])) {
        LOGE("Failed to create hand pose space for right hand.");
    }
}

bool attachActionSet() {
    XrSessionActionSetsAttachInfo actionSetsAttachInfo = {XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    actionSetsAttachInfo.countActionSets = 1;
    actionSetsAttachInfo.actionSets = &actionSet;
    XR_FAILRETURN(xrAttachSessionActionSets(xrinfo.session, &actionSetsAttachInfo), false);
    return true;
}

bool pollActions(XrTime predictedTime) {
    XrActiveActionSet activeActionSet = {};
    activeActionSet.actionSet = actionSet;
    activeActionSet.subactionPath = XR_NULL_PATH;
    XrActionsSyncInfo actionsSyncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
    actionsSyncInfo.countActiveActionSets = 1;
    actionsSyncInfo.activeActionSets = &activeActionSet;
    XrResult xrSyncActions_result = xrSyncActions(xrinfo.session, &actionsSyncInfo);
    if(xrSyncActions_result == XR_SESSION_NOT_FOCUSED) {
        // If this happens we don't need to log it every frame
        return false;
    }
    // Otherwise, we can log it and return false
    XR_FAILRETURN(xrSyncActions_result, false);

    XrActionStateGetInfo actionStateGetInfo = {XR_TYPE_ACTION_STATE_GET_INFO};
    actionStateGetInfo.action = palmPoseAction;
    for (int i = 0; i < 2; i++) {
        actionStateGetInfo.subactionPath = handPaths[i];
        // Probably should memcpy defaults here in the future if this causes problems
        XR_FAILRETURN(xrGetActionStatePose(xrinfo.session, &actionStateGetInfo, &handPoseState[i]), false);
        if (handPoseState[i].isActive) {
            XrSpaceLocation spaceLocation = {XR_TYPE_SPACE_LOCATION};
            XrResult res = xrLocateSpace(handPoseSpace[i], xrinfo.localReferenceSpace, predictedTime, &spaceLocation);
            if (XR_UNQUALIFIED_SUCCESS(res) &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (spaceLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                xrInput.handPose[i] = spaceLocation.pose;
            } else {
                handPoseState[i].isActive = false;
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        vibrate[i] *= 0.5f;
        if (vibrate[i] < 0.01f)
            vibrate[i] = 0.0f;
        XrHapticVibration vibration = {XR_TYPE_HAPTIC_VIBRATION};
        vibration.amplitude = vibrate[i];
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo = {XR_TYPE_HAPTIC_ACTION_INFO};
        hapticActionInfo.action = vibrateAction;
        hapticActionInfo.subactionPath = handPaths[i];
        XR_FAILRETURN(xrApplyHapticFeedback(xrinfo.session, &hapticActionInfo, (XrHapticBaseHeader *)&vibration), false);
    }

    actionStateGetInfo.action = clickScreenAction;
    for (int i = 0; i < 2; i++) {
        actionStateGetInfo.subactionPath = handPaths[i];
        XR_FAILRETURN(xrGetActionStateBoolean(xrinfo.session, &actionStateGetInfo, &clickScreenState[i]), false);
        if (clickScreenState[i].isActive && clickScreenState[i].changedSinceLastSync) {
            float u, v;
            if (controllerRayIntersectsScreen(i, &u, &v)) {
                vibrate[i] = 1.0f;
                clickScreenAtPosition(getJniEnv(), u, v);
            }
        }
    }

    return true;
}

bool controllerRayIntersectsScreen(int controllerIndex, float* u, float* v) {
    XrVector3f rayStart, rayEnd;
    getControllerRay(controllerIndex, ((UboViewData*)vk_rs.uniformMappedData)->modelMatrix, &rayStart, &rayEnd);

    XrVector3f rayDir;
    XrVector3f_Sub(&rayDir, &rayEnd, &rayStart);
    XrVector3f_Normalize(&rayDir);

    float tempU, tempV, t;
    bool hit = false;
    float finalU = 0, finalV = 0;

    if (rayIntersectsTriangle(rayStart, rayDir, screenTri1, &tempU, &tempV, &t)) {
        finalU = 1.0f - (tempU + tempV);
        finalV = tempV;
        hit = true;
    } else if (rayIntersectsTriangle(rayStart, rayDir, screenTri2, &tempU, &tempV, &t)) {
        finalU = 1.0f - tempU;
        finalV = tempU + tempV;
        hit = true;
    }

    if (hit) {
        *u = 1.0f - finalU;
        *v = finalV;
    }

    return hit;
}

bool rayIntersectsTriangle(XrVector3f origin, XrVector3f direction, XrVector3f* tri, float* u, float* v, float* t) {
    const float EPSILON = 0.0000001f;
    XrVector3f edge1 = { tri[1].x - tri[0].x, tri[1].y - tri[0].y, tri[1].z - tri[0].z };
    XrVector3f edge2 = { tri[2].x - tri[0].x, tri[2].y - tri[0].y, tri[2].z - tri[0].z };

    XrVector3f h = { direction.y * edge2.z - direction.z * edge2.y,
                     direction.z * edge2.x - direction.x * edge2.z,
                     direction.x * edge2.y - direction.y * edge2.x };
    float a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;

    if (a > -EPSILON && a < EPSILON) return false;

    float f = 1.0f / a;
    XrVector3f s = { origin.x - tri[0].x, origin.y - tri[0].y, origin.z - tri[0].z };
    (*u) = f * (s.x * h.x + s.y * h.y + s.z * h.z);

    if ((*u) < 0.0f || (*u) > 1.0f) return false;

    XrVector3f q = { s.y * edge1.z - s.z * edge1.y,
                     s.z * edge1.x - s.x * edge1.z,
                     s.x * edge1.y - s.y * edge1.x };
    (*v) = f * (direction.x * q.x + direction.y * q.y + direction.z * q.z);

    if ((*v) < 0.0f || (*u) + (*v) > 1.0f) return false;

    (*t) = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);
    return ((*t) > EPSILON);
}

void getControllerRay(int controller, XrMatrix4x4f model, XrVector3f* startOut, XrVector3f* endOut) {
    XrMatrix4x4f inverted;
    XrMatrix4x4f_Invert(&inverted, &model);

    XrPosef hand = xrInput.handPose[controller];
    XrVector3f worldSpace;
    XrMatrix4x4f_TransformVector3f(&worldSpace, &inverted, &hand.position);

    XrVector3f start = worldSpace;

    XrVector3f direction = {0, 1, 1};

    XrMatrix4x4f orientation;
    XrMatrix4x4f_CreateFromQuaternion(&orientation, &hand.orientation);

    XrVector3f result;
    XrMatrix4x4f_TransformVector3f(&result, &orientation, &direction);
    result.y = -result.y;

    XrVector3f end;
    XrVector3f_Add(&end, &result, &start);
    startOut->x = start.x;
    startOut->y = start.y;
    startOut->z = start.z;
    endOut->x = end.x;
    endOut->y = end.y;
    endOut->z = end.z;
}
