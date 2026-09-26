/*
 * EGL/egl.h - Khronos EGL Standard Header
 */

#ifndef __egl_h_
#define __egl_h_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLClientBuffer;
typedef void (*__eglMustCastToProperFunctionPointerType)(void);

#define EGL_FALSE                       0
#define EGL_TRUE                        1
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)
#define EGL_DEFAULT_DISPLAY             ((EGLNativeDisplayType)0)

#define EGL_SUCCESS                     0x3000
#define EGL_BUFFER_SIZE                 0x3020
#define EGL_RED_SIZE                    0x3024
#define EGL_GREEN_SIZE                  0x3023
#define EGL_BLUE_SIZE                   0x3022
#define EGL_ALPHA_SIZE                  0x3021
#define EGL_DEPTH_SIZE                  0x3025
#define EGL_STENCIL_SIZE                0x3026
#define EGL_SURFACE_TYPE                0x3033
#define EGL_WINDOW_BIT                  0x0004
#define EGL_RENDERABLE_TYPE             0x3040
#define EGL_OPENGL_BIT                  0x0008
#define EGL_OPENGL_ES2_BIT              0x0004
#define EGL_NONE                        0x3038
#define EGL_CONTEXT_CLIENT_VERSION      0x3098
#define EGL_OPENGL_API                  0x30A2
#define EGL_OPENGL_ES_API               0x30A0
#define EGL_PLATFORM_GBM_KHR            0x31D7

EGLDisplay eglGetDisplay(void *display_id);
EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
EGLBoolean eglTerminate(EGLDisplay dpy);
EGLBoolean eglBindAPI(EGLenum api);
EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                           EGLConfig *configs, EGLint config_size,
                           EGLint *num_config);
EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context,
                            const EGLint *attrib_list);
EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                  void *win, const EGLint *attrib_list);
EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface);
EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx);
EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                          EGLSurface read, EGLContext ctx);
EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
__eglMustCastToProperFunctionPointerType eglGetProcAddress(const char *procname);

#ifdef __cplusplus
}
#endif

#endif /* __egl_h_ */
