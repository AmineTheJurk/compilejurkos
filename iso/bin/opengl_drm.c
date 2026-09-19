/*
 * opengl_drm.c - Standard Bare-Metal OpenGL on Linux DRM/KMS
 *
 * Implements full 3D hardware-accelerated OpenGL directly on top of the
 * standard Linux Direct Rendering Manager (DRM/KMS), Mesa Generic Buffer
 * Management (GBM), and Khronos EGL.
 *
 * Runs without X11 or Wayland on modern Linux GPU drivers (Intel, AMD,
 * NVIDIA nouveau, VirtIO GPU, Bochs DRM, etc.).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/select.h>
#include <math.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <GL/gl.h>

struct drm_kms_state {
    int fd;
    drmModeRes *resources;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo mode;
    uint32_t crtc_id;
    drmModeCrtc *orig_crtc;
};

struct gbm_state {
    struct gbm_device *dev;
    struct gbm_surface *surface;
};

struct egl_state {
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;
};

static struct termios orig_termios;

static void reset_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[?25h\033[0m\n");
    fflush(stdout);
}

static void set_raw_terminal(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(reset_terminal);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\033[?25l");
    fflush(stdout);
}

static int check_exit_key(void) {
    char ch;
    if (read(STDIN_FILENO, &ch, 1) > 0) {
        if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
            return 1;
        }
    }
    return 0;
}

static int init_drm(struct drm_kms_state *drm) {
    char path[32];
    drm->fd = -1;

    for (int i = 0; i < DRM_MAX_MINOR; i++) {
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        drm->fd = open(path, O_RDWR | O_CLOEXEC);
        if (drm->fd >= 0) break;
    }

    if (drm->fd < 0) {
        fprintf(stderr, "[DRM/KMS] Failed to open /dev/dri/card*.\n");
        return -1;
    }

    drm->resources = drmModeGetResources(drm->fd);
    if (!drm->resources) {
        fprintf(stderr, "[DRM/KMS] drmModeGetResources failed.\n");
        close(drm->fd);
        return -1;
    }

    // Find first connected display
    for (int i = 0; i < drm->resources->count_connectors; i++) {
        drm->connector = drmModeGetConnector(drm->fd, drm->resources->connectors[i]);
        if (drm->connector && drm->connector->connection == 1 && drm->connector->count_modes > 0) {
            break;
        }
        if (drm->connector) {
            drmModeFreeConnector(drm->connector);
            drm->connector = NULL;
        }
    }

    if (!drm->connector) {
        fprintf(stderr, "[DRM/KMS] No connected display connector found.\n");
        drmModeFreeResources(drm->resources);
        close(drm->fd);
        return -1;
    }

    drm->mode = drm->connector->modes[0];

    if (drm->connector->encoder_id) {
        drm->encoder = drmModeGetEncoder(drm->fd, drm->connector->encoder_id);
    }
    if (drm->encoder && drm->encoder->crtc_id) {
        drm->crtc_id = drm->encoder->crtc_id;
    } else if (drm->resources->count_crtcs > 0) {
        drm->crtc_id = drm->resources->crtcs[0];
    }

    drm->orig_crtc = drmModeGetCrtc(drm->fd, drm->crtc_id);
    return 0;
}

static int init_gbm_egl(struct drm_kms_state *drm, struct gbm_state *gbm, struct egl_state *egl) {
    gbm->dev = gbm_create_device(drm->fd);
    if (!gbm->dev) {
        fprintf(stderr, "[GBM] Failed to create GBM device.\n");
        return -1;
    }

    gbm->surface = gbm_surface_create(gbm->dev,
                                      drm->mode.hdisplay,
                                      drm->mode.vdisplay,
                                      GBM_FORMAT_XRGB8888,
                                      GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!gbm->surface) {
        fprintf(stderr, "[GBM] Failed to create GBM surface.\n");
        return -1;
    }

    egl->display = eglGetDisplay((void *)gbm->dev);
    if (egl->display == EGL_NO_DISPLAY) {
        fprintf(stderr, "[EGL] eglGetDisplay failed.\n");
        return -1;
    }

    EGLint major, minor;
    if (!eglInitialize(egl->display, &major, &minor)) {
        fprintf(stderr, "[EGL] eglInitialize failed.\n");
        return -1;
    }

    eglBindAPI(EGL_OPENGL_API);

    static const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    if (!eglChooseConfig(egl->display, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        fprintf(stderr, "[EGL] eglChooseConfig failed.\n");
        return -1;
    }

    egl->context = eglCreateContext(egl->display, config, EGL_NO_CONTEXT, NULL);
    if (egl->context == EGL_NO_CONTEXT) {
        fprintf(stderr, "[EGL] eglCreateContext failed.\n");
        return -1;
    }

    egl->surface = eglCreateWindowSurface(egl->display, config, (void *)gbm->surface, NULL);
    if (egl->surface == EGL_NO_SURFACE) {
        fprintf(stderr, "[EGL] eglCreateWindowSurface failed.\n");
        return -1;
    }

    if (!eglMakeCurrent(egl->display, egl->surface, egl->surface, egl->context)) {
        fprintf(stderr, "[EGL] eglMakeCurrent failed.\n");
        return -1;
    }

    return 0;
}

static void draw_cube(void) {
    glBegin(GL_QUADS);

    // Front (Red)
    glColor3f(0.95f, 0.25f, 0.25f);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);

    // Back (Green)
    glColor3f(0.25f, 0.95f, 0.25f);
    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);

    // Top (Blue)
    glColor3f(0.25f, 0.45f, 0.95f);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);

    // Bottom (Yellow)
    glColor3f(0.95f, 0.95f, 0.25f);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);

    // Right (Magenta)
    glColor3f(0.95f, 0.25f, 0.95f);
    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f( 1.0f, -1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f, -1.0f);
    glVertex3f( 1.0f,  1.0f,  1.0f);
    glVertex3f( 1.0f, -1.0f,  1.0f);

    // Left (Cyan)
    glColor3f(0.25f, 0.95f, 0.95f);
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-1.0f, -1.0f, -1.0f);
    glVertex3f(-1.0f, -1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f,  1.0f);
    glVertex3f(-1.0f,  1.0f, -1.0f);

    glEnd();
}

int main(int argc, char *argv[]) {
    printf("\033[2J\033[H");
    printf("\033[1;36m[JurkOS OpenGL on DRM/KMS Subsystem]\033[0m\n");
    printf("Initializing Direct Rendering Manager (DRM), GBM, and EGL...\n");

    struct drm_kms_state drm;
    memset(&drm, 0, sizeof(drm));
    if (init_drm(&drm) < 0) {
        return 1;
    }

    struct gbm_state gbm;
    struct egl_state egl;
    memset(&gbm, 0, sizeof(gbm));
    memset(&egl, 0, sizeof(egl));

    if (init_gbm_egl(&drm, &gbm, &egl) < 0) {
        return 1;
    }

    printf("DRM Resolution: %ux%u @ %u Hz (Connector: %u, CRTC: %u)\n",
           drm.mode.hdisplay, drm.mode.vdisplay,
           drm.mode.vrefresh ? drm.mode.vrefresh : 60,
           drm.connector->connector_id, drm.crtc_id);
    printf("OpenGL API Active: Hardware Accelerated 3D Rendering\n");
    printf("Press [Q] or [ESC] to exit.\n");
    sleep(1);

    set_raw_terminal();

    // Setup OpenGL Viewport & State
    glViewport(0, 0, drm.mode.hdisplay, drm.mode.vdisplay);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)drm.mode.hdisplay / (float)drm.mode.vdisplay;
    glScalef(1.0f / aspect, 1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float angle_x = 0.0f;
    float angle_y = 0.0f;
    int running = 1;
    uint32_t fb_id = 0;
    struct gbm_bo *bo = NULL;

    while (running) {
        if (check_exit_key()) {
            running = 0;
            break;
        }

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -4.0f);
        glRotatef(angle_x, 1.0f, 0.0f, 0.0f);
        glRotatef(angle_y, 0.0f, 1.0f, 0.0f);
        glScalef(0.6f, 0.6f, 0.6f);

        draw_cube();
        glPopMatrix();

        eglSwapBuffers(egl.display, egl.surface);

        struct gbm_bo *next_bo = gbm_surface_lock_front_buffer(gbm.surface);
        if (next_bo) {
            uint32_t handle = gbm_bo_get_handle(next_bo).u32;
            uint32_t stride = gbm_bo_get_stride(next_bo);
            uint32_t next_fb = 0;

            if (drmModeAddFB(drm.fd, drm.mode.hdisplay, drm.mode.vdisplay, 24, 32, stride, handle, &next_fb) == 0) {
                drmModeSetCrtc(drm.fd, drm.crtc_id, next_fb, 0, 0, &drm.connector->connector_id, 1, &drm.mode);
                if (fb_id) drmModeRmFB(drm.fd, fb_id);
                fb_id = next_fb;
            }
            if (bo) gbm_surface_release_buffer(gbm.surface, bo);
            bo = next_bo;
        }

        angle_x += 1.2f;
        angle_y += 1.8f;
        usleep(16000); // ~60 FPS
    }

    // Cleanup & Restore
    if (drm.orig_crtc) {
        drmModeSetCrtc(drm.fd, drm.orig_crtc->crtc_id, drm.orig_crtc->buffer_id,
                       drm.orig_crtc->x, drm.orig_crtc->y,
                       &drm.connector->connector_id, 1, &drm.orig_crtc->mode);
        drmModeFreeCrtc(drm.orig_crtc);
    }
    if (fb_id) drmModeRmFB(drm.fd, fb_id);
    if (bo) gbm_surface_release_buffer(gbm.surface, bo);

    eglDestroySurface(egl.display, egl.surface);
    eglDestroyContext(egl.display, egl.context);
    eglTerminate(egl.display);
    gbm_surface_destroy(gbm.surface);
    gbm_device_destroy(gbm.dev);

    if (drm.encoder) drmModeFreeEncoder(drm.encoder);
    if (drm.connector) drmModeFreeConnector(drm.connector);
    if (drm.resources) drmModeFreeResources(drm.resources);
    if (drm.fd >= 0) close(drm.fd);

    printf("\033[2J\033[HExited OpenGL on DRM/KMS.\n");
    return 0;
}
