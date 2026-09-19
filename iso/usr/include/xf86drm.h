/*
 * xf86drm.h - Standard Direct Rendering Manager (libdrm) User-Space Header
 * Part of the freedesktop.org / X.Org Foundation DRM ecosystem.
 */

#ifndef _XF86DRM_H_
#define _XF86DRM_H_

#include <stddef.h>
#include <stdint.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DRM_MAX_MINOR   16

int           drmOpen(const char *name, const char *busid);
int           drmClose(int fd);
int           drmGetCap(int fd, uint64_t capability, uint64_t *value);
int           drmSetClientCap(int fd, uint64_t capability, uint64_t value);
int           drmIoctl(int fd, unsigned long request, void *arg);
char         *drmGetDeviceNameFromFd2(int fd);
int           drmDropMaster(int fd);
int           drmSetMaster(int fd);
int           drmIsMaster(int fd);
int           drmHandleEvent(int fd, void *evctx);

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRM_H_ */
