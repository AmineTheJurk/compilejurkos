/*
 * gbm.h - Mesa Generic Buffer Management Header
 * Used for bare-metal OpenGL/EGL surface creation on Linux DRM/KMS.
 */

#ifndef _GBM_H_
#define _GBM_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gbm_device;
struct gbm_bo;
struct gbm_surface;

enum gbm_bo_format {
    GBM_FORMAT_XRGB8888 = 0x34325258,
    GBM_FORMAT_ARGB8888 = 0x34325241,
};

enum gbm_bo_flags {
    GBM_BO_USE_SCANOUT      = (1 << 0),
    GBM_BO_USE_CURSOR       = (1 << 1),
    GBM_BO_USE_RENDERING    = (1 << 2),
    GBM_BO_USE_WRITE        = (1 << 3),
    GBM_BO_USE_LINEAR       = (1 << 4),
};

union gbm_bo_handle {
    void *ptr;
    int32_t s32;
    uint32_t u32;
    int64_t s64;
    uint64_t u64;
};

struct gbm_device *gbm_create_device(int fd);
void gbm_device_destroy(struct gbm_device *gbm);
struct gbm_surface *gbm_surface_create(struct gbm_device *gbm,
                                       uint32_t width, uint32_t height,
                                       uint32_t format, uint32_t flags);
void gbm_surface_destroy(struct gbm_surface *surf);
struct gbm_bo *gbm_surface_lock_front_buffer(struct gbm_surface *surf);
void gbm_surface_release_buffer(struct gbm_surface *surf, struct gbm_bo *bo);
uint32_t gbm_bo_get_width(struct gbm_bo *bo);
uint32_t gbm_bo_get_height(struct gbm_bo *bo);
uint32_t gbm_bo_get_stride(struct gbm_bo *bo);
uint32_t gbm_bo_get_format(struct gbm_bo *bo);
union gbm_bo_handle gbm_bo_get_handle(struct gbm_bo *bo);
void gbm_bo_set_user_data(struct gbm_bo *bo, void *data, void (*destroy_user_data)(struct gbm_bo *, void *));
void *gbm_bo_get_user_data(struct gbm_bo *bo);

#ifdef __cplusplus
}
#endif

#endif /* _GBM_H_ */
