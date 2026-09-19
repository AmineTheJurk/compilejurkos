CC ?= gcc
CFLAGS ?= -O2 -Wall -I../../iso/usr/include
LIBS ?= -ldrm -lgbm -lEGL -lGL -lm

all: opengl_drm

opengl_drm: ../../iso/bin/opengl_drm.c
	$(CC) $(CFLAGS) $< $(LIBS) -o $@
	cp -f $@ ../../iso/bin/opengl_drm
	chmod +x ../../iso/bin/opengl_drm
	ln -sf opengl_drm ../../iso/bin/opengl

clean:
	rm -f opengl_drm
