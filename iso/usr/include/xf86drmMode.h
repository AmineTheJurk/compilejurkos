/*
 * xf86drmMode.h - Standard Kernel Mode Setting (KMS) Header (libdrm)
 * Part of the freedesktop.org / X.Org Foundation DRM ecosystem.
 */

#ifndef _XF86DRMMODE_H_
#define _XF86DRMMODE_H_

#include <stddef.h>
#include <stdint.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _drmModeRes {
    int count_fbs;
    uint32_t *fbs;
    int count_crtcs;
    uint32_t *crtcs;
    int count_connectors;
    uint32_t *connectors;
    int count_encoders;
    uint32_t *encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;

typedef struct _drmModeModeInfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char name[DRM_DISPLAY_MODE_LEN];
} drmModeModeInfo, *drmModeModeInfoPtr;

typedef struct _drmModeConnector {
    uint32_t connector_id;
    uint32_t encoder_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mmWidth, mmHeight;
    uint32_t subpixel;
    int count_modes;
    drmModeModeInfoPtr modes;
    int count_props;
    uint32_t *props;
    uint64_t *prop_values;
    int count_encoders;
    uint32_t *encoders;
} drmModeConnector, *drmModeConnectorPtr;

typedef struct _drmModeEncoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
} drmModeEncoder, *drmModeEncoderPtr;

typedef struct _drmModeCrtc {
    uint32_t crtc_id;
    uint32_t buffer_id;
    uint32_t x, y;
    uint32_t width, height;
    int mode_valid;
    drmModeModeInfo mode;
    int gamma_size;
} drmModeCrtc, *drmModeCrtcPtr;

typedef struct _drmModeFB {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
} drmModeFB, *drmModeFBPtr;

/* Function Declarations */
drmModeResPtr       drmModeGetResources(int fd);
void                drmModeFreeResources(drmModeResPtr ptr);
drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connectorId);
void                drmModeFreeConnector(drmModeConnectorPtr ptr);
drmModeEncoderPtr   drmModeGetEncoder(int fd, uint32_t encoder_id);
void                drmModeFreeEncoder(drmModeEncoderPtr ptr);
drmModeCrtcPtr      drmModeGetCrtc(int fd, uint32_t crtcId);
void                drmModeFreeCrtc(drmModeCrtcPtr ptr);
int                 drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                                   uint32_t x, uint32_t y, uint32_t *connectors, int count,
                                   drmModeModeInfoPtr mode);
int                 drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
                                 uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                                 uint32_t *buf_id);
int                 drmModeRmFB(int fd, uint32_t bufferId);
int                 drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
                                    uint32_t flags, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRMMODE_H_ */
