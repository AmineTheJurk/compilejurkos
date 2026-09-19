/*
 * fbgfx.c - Direct Framebuffer Graphics Demonstration for JurkOS
 * Renders smooth animated geometric graphics & plasma directly to /dev/fb0.
 * Compatible with standard Linux framebuffer drivers (VESA / EFI / SimpleFB / DRM fbdev).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <termios.h>

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

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

int main(int argc, char *argv[]) {
    int fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("Error opening /dev/fb0 (Framebuffer device not found or DRM/VESA not loaded)");
        return 1;
    }

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0 ||
        ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0) {
        perror("Error reading framebuffer information");
        close(fb_fd);
        return 1;
    }

    size_t screensize = finfo.line_length * vinfo.yres;
    uint8_t *fbp = (uint8_t *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fbp == MAP_FAILED) {
        perror("Error mapping framebuffer device to memory");
        close(fb_fd);
        return 1;
    }

    uint32_t *backbuffer = (uint32_t *)malloc(vinfo.xres * vinfo.yres * sizeof(uint32_t));
    if (!backbuffer) {
        fprintf(stderr, "Out of memory for backbuffer\n");
        munmap(fbp, screensize);
        close(fb_fd);
        return 1;
    }

    set_raw_terminal();

    printf("\033[2J\033[H");
    printf("\033[1;36m[JurkOS Framebuffer Graphics Demo]\033[0m\n");
    printf("Resolution: %dx%d (%d bpp)\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    printf("Press [Q] or [Ctrl+C] to exit back to shell.\n");
    sleep(1);

    int width = vinfo.xres;
    int height = vinfo.yres;
    int bpp = vinfo.bits_per_pixel;
    int cx = width / 2;
    int cy = height / 2;

    float t = 0.0f;
    int running = 1;

    while (running) {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) {
            if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
                running = 0;
                break;
            }
        }

        // Render animated geometric plasma scene into backbuffer
        for (int y = 0; y < height; y += 2) {
            float dy = (float)(y - cy);
            for (int x = 0; x < width; x += 2) {
                float dx = (float)(x - cx);
                float dist = sqrtf(dx * dx + dy * dy);

                float v1 = sinf(dx * 0.03f + t);
                float v2 = sinf(dy * 0.03f - t * 0.7f);
                float v3 = sinf((dist * 0.05f) + t * 1.5f);
                float sum = (v1 + v2 + v3) / 3.0f;

                uint8_t r = (uint8_t)((sinf(sum * 3.1415f + t) * 0.5f + 0.5f) * 255.0f);
                uint8_t g = (uint8_t)((cosf(sum * 3.1415f + t * 0.5f) * 0.5f + 0.5f) * 255.0f);
                uint8_t b = (uint8_t)((sinf(dist * 0.02f - t) * 0.5f + 0.5f) * 255.0f);

                uint32_t col = rgb(r, g, b);
                backbuffer[y * width + x] = col;
                backbuffer[y * width + (x + 1)] = col;
                backbuffer[(y + 1) * width + x] = col;
                backbuffer[(y + 1) * width + (x + 1)] = col;
            }
        }

        // Render rotating wireframe ring in center
        int ring_points = 32;
        float radius = 120.0f + sinf(t * 2.0f) * 30.0f;
        for (int i = 0; i < ring_points; i++) {
            float angle = (i * 2.0f * 3.14159f / ring_points) + t;
            int px = cx + (int)(cosf(angle) * radius);
            int py = cy + (int)(sinf(angle) * (radius * 0.6f));

            if (px >= 2 && px < width - 2 && py >= 2 && py < height - 2) {
                for (int dy = -2; dy <= 2; dy++) {
                    for (int dx = -2; dx <= 2; dx++) {
                        backbuffer[(py + dy) * width + (px + dx)] = 0x00FFFFFF;
                    }
                }
            }
        }

        // Copy backbuffer to framebuffer memory
        if (bpp == 32) {
            for (int y = 0; y < height; y++) {
                uint32_t *dest = (uint32_t *)(fbp + y * finfo.line_length);
                memcpy(dest, &backbuffer[y * width], width * sizeof(uint32_t));
            }
        } else if (bpp == 16) {
            for (int y = 0; y < height; y++) {
                uint16_t *dest = (uint16_t *)(fbp + y * finfo.line_length);
                for (int x = 0; x < width; x++) {
                    uint32_t c = backbuffer[y * width + x];
                    uint16_t r5 = (c >> 19) & 0x1F;
                    uint16_t g6 = (c >> 10) & 0x3F;
                    uint16_t b5 = (c >> 3) & 0x1F;
                    dest[x] = (r5 << 11) | (g6 << 5) | b5;
                }
            }
        }

        t += 0.06f;
        usleep(16000); // ~60 FPS
    }

    // Clear screen black before exiting
    memset(fbp, 0, screensize);

    free(backbuffer);
    munmap(fbp, screensize);
    close(fb_fd);
    printf("\033[2J\033[HExited Framebuffer Graphics Demo.\n");
    return 0;
}
