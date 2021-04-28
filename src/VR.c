#include "VR.h"

#include <GL/glew.h>
#include <cglm/struct.h>
#include <openvr_capi.h>
#include <stdint.h>
#include <stdio.h>

#include "Constants.h"
#include "Game.h"
#include "Graphics.h"
#include "Logger.h"
#include "Platform.h"
#include "String.h"
#include "VRInternal.h"
#include "Vectors.h"
#include "Window.h"

mat4s m_rmat4DevicePose[64 /* k_unMaxTrackedDeviceCount */];
mat4s m_mat4HMDPose;

void VR_UpdateHMDMatrixPose() {
  EVRCompositorError error = g_pCompositor->WaitGetPoses(
      g_rTrackedDevicePose, k_unMaxTrackedDeviceCount, NULL, 0);
  if (error != EVRCompositorError_VRCompositorError_None &&
      error != EVRCompositorError_VRCompositorError_DoNotHaveFocus) {
    Logger_Abort2(error, "WaitGetPoses");
    return;
  }

  for (unsigned int nDevice = 0; nDevice < k_unMaxTrackedDeviceCount;
       ++nDevice) {
    if (g_rTrackedDevicePose[nDevice].bPoseIsValid) {
      m_rmat4DevicePose[nDevice] = Mat4sFromHmdMatrix34(
          g_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
    }
  }

  if (g_rTrackedDevicePose[k_unTrackedDeviceIndex_Hmd].bPoseIsValid) {
    m_mat4HMDPose = m_rmat4DevicePose[k_unTrackedDeviceIndex_Hmd];
    m_mat4HMDPose = glms_mat4_inv(m_mat4HMDPose);
  }
}

void VR_Setup() {
  char errorMessage[256];

  glewExperimental = GL_TRUE;
  GLenum nGlewError = glewInit();
  if (nGlewError != GLEW_OK) {
    snprintf(errorMessage, sizeof(errorMessage), "glewInit: %s",
             glewGetErrorString(nGlewError));
    Logger_Abort2(nGlewError, errorMessage);
    return;
  }
  glGetError();  // to clear the error caused deep in GLEW

  EVRInitError error;
  VR_InitInternal(&error, EVRApplicationType_VRApplication_Scene);
  if (error != EVRInitError_VRInitError_None) {
    snprintf(errorMessage, sizeof(errorMessage), "VR_InitInternal: %s",
             VR_GetStringForHmdError(error));
    Logger_Abort2(error, errorMessage);
    return;
  }

  if (!VR_IsInterfaceVersionValid(IVRSystem_Version)) {
    VR_ShutdownInternal();
    Logger_Abort("!VR_IsInterfaceVersionValid System");
    return;
  }

  // get System interface
  char interfaceName[256];
  snprintf(interfaceName, sizeof(interfaceName), "FnTable:%s",
           IVRSystem_Version);
  g_pSystem = (struct VR_IVRSystem_FnTable*)VR_GetGenericInterface(
      interfaceName, &error);
  if (error != EVRInitError_VRInitError_None) {
    snprintf(errorMessage, sizeof(errorMessage),
             "VR_GetGenericInterface System: %s",
             VR_GetStringForHmdError(error));
    Logger_Abort2(error, errorMessage);
    return;
  }

  // get Compositor interface
  snprintf(interfaceName, sizeof(interfaceName), "FnTable:%s",
           IVRCompositor_Version);
  g_pCompositor = (struct VR_IVRCompositor_FnTable*)VR_GetGenericInterface(
      interfaceName, &error);
  if (error != EVRInitError_VRInitError_None) {
    snprintf(errorMessage, sizeof(errorMessage),
             "VR_GetGenericInterface Compositor: %s",
             VR_GetStringForHmdError(error));
    Logger_Abort2(error, errorMessage);
    return;
  }

  // set window title
  cc_string title;
  char titleBuffer[STRING_SIZE];
  String_InitArray(title, titleBuffer);

  char headsetName[256];
  ETrackedPropertyError propError;
  g_pSystem->GetStringTrackedDeviceProperty(
      k_unTrackedDeviceIndex_Hmd,
      ETrackedDeviceProperty_Prop_TrackingSystemName_String, headsetName,
      sizeof(headsetName), &propError);
  if (propError != ETrackedPropertyError_TrackedProp_Success) {
    Logger_Abort2(propError,
                  "GetStringTrackedDeviceProperty "
                  "ETrackedDeviceProperty_Prop_TrackingSystemName_String");
    return;
  }
  String_Format3(&title, "%c (%s) - %c", GAME_APP_TITLE, &Game_Username,
                 &headsetName);
  Window_SetTitle(&title);

  SetupCameras();
  SetupStereoRenderTargets();
}

void RenderCompanionWindow() {
  Gfx_Begin2D(Game.Width, Game.Height);
  glViewport(0, 0, Game.Width, Game.Height);
  Gfx_SetTexturing(true);
  struct Texture tex;

  // render left eye (first half of index array )
  tex.ID = g_descLeftEye.m_nRenderTextureId;
  tex.X = 0;
  tex.Y = 0;
  tex.Width = Game.Width / 2;
  tex.Height = Game.Height;
  tex.uv.U1 = 0;
  tex.uv.U2 = 1;
  tex.uv.V1 = 1;
  tex.uv.V2 = 0;
  Texture_Render(&tex);

  // render right eye (second half of index array )
  tex.ID = g_descRightEye.m_nRenderTextureId;
  tex.X = Game.Width / 2;
  tex.Y = 0;
  tex.Width = Game.Width / 2;
  tex.Height = Game.Height;
  tex.uv.U1 = 0;
  tex.uv.U2 = 1;
  tex.uv.V1 = 1;
  tex.uv.V2 = 0;
  Texture_Render(&tex);

  Gfx_SetTexturing(false);
  Gfx_End2D();
}

void VR_RenderStereoTargets(void (*RenderScene)(Hmd_Eye nEye,
                                                double delta,
                                                float t),
                            double delta,
                            float t) {
  // Left Eye
  glBindFramebuffer(GL_FRAMEBUFFER, g_descLeftEye.m_nRenderFramebufferId);
  glViewport(0, 0, g_nRenderWidth, g_nRenderHeight);
  RenderScene(EVREye_Eye_Left, delta, t);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Right Eye
  glBindFramebuffer(GL_FRAMEBUFFER, g_descRightEye.m_nRenderFramebufferId);
  glViewport(0, 0, g_nRenderWidth, g_nRenderHeight);
  RenderScene(EVREye_Eye_Right, delta, t);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void VR_BeginFrame() {
  //
}

void VR_EndFrame() {
  // end RenderStereoTargets();

  RenderCompanionWindow();

  // after RenderCompanionWindow();
  EVRCompositorError error;
  Texture_t leftEyeTexture = {
      (void*)(uintptr_t)g_descLeftEye.m_nRenderTextureId,
      ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Gamma};
  error = g_pCompositor->Submit(EVREye_Eye_Left, &leftEyeTexture, NULL,
                                EVRSubmitFlags_Submit_Default);
  if (error != EVRCompositorError_VRCompositorError_None &&
      error != EVRCompositorError_VRCompositorError_DoNotHaveFocus) {
    Logger_Abort2(error, "Submit EVREye_Eye_Left");
    return;
  }

  Texture_t rightEyeTexture = {
      (void*)(uintptr_t)g_descRightEye.m_nRenderTextureId,
      ETextureType_TextureType_OpenGL, EColorSpace_ColorSpace_Gamma};
  error = g_pCompositor->Submit(EVREye_Eye_Right, &rightEyeTexture, NULL,
                                EVRSubmitFlags_Submit_Default);
  if (error != EVRCompositorError_VRCompositorError_None &&
      error != EVRCompositorError_VRCompositorError_DoNotHaveFocus) {
    Logger_Abort2(error, "Submit EVREye_Eye_Right");
    return;
  }
}

struct Matrix VR_GetViewMatrix() {
  return MatrixFromMat4s(m_mat4HMDPose);
}

struct Matrix VR_GetProjectionMatrix(Hmd_Eye nEye) {
  mat4s m;

  if (nEye == EVREye_Eye_Left) {
    m = glms_mat4_mul(g_mat4ProjectionLeft, g_mat4eyePosLeft);
  } else if (nEye == EVREye_Eye_Right) {
    m = glms_mat4_mul(g_mat4ProjectionRight, g_mat4eyePosRight);
  } else {
    m = glms_mat4_identity();
  }

  return MatrixFromMat4s(m);
}

void VR_Shutdown() {
  VR_ShutdownInternal();
}
