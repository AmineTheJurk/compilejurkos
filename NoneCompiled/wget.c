/* JurkOS Bare-Metal wget (powered by BearSSL)
 * Copyright (c) 2026 AmineTheJurk
 * Standalone HTTP and HTTPS downloader with zero external shared libraries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "bearssl/inc/bearssl.h"

/* No-anchor x509 wrapper for bare-metal HTTPS */
typedef struct {
    const br_x509_class *vtable;
    const br_x509_class **inner;
} x509_noanchor_context;

static void xwc_start_chain(const br_x509_class **ctx, const char *server_name) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->start_chain(xwc->inner, server_name);
}

static void xwc_start_cert(const br_x509_class **ctx, uint32_t length) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->start_cert(xwc->inner, length);
}

static void xwc_append(const br_x509_class **ctx, const unsigned char *buf, size_t len) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->append(xwc->inner, buf, len);
}

static void xwc_end_cert(const br_x509_class **ctx) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    (*xwc->inner)->end_cert(xwc->inner);
}

static unsigned xwc_end_chain(const br_x509_class **ctx) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    unsigned r = (*xwc->inner)->end_chain(xwc->inner);
    if (r == BR_ERR_X509_NOT_TRUSTED) {
        r = 0;
    }
    return r;
}

static const br_x509_pkey *xwc_get_pkey(const br_x509_class *const *ctx, unsigned *usages) {
    x509_noanchor_context *xwc = (x509_noanchor_context *)ctx;
    return (*xwc->inner)->get_pkey(xwc->inner, usages);
}

static const br_x509_class x509_noanchor_vtable = {
    sizeof(x509_noanchor_context),
    xwc_start_chain,
    xwc_start_cert,
    xwc_append,
    xwc_end_cert,
    xwc_end_chain,
    xwc_get_pkey
};

static void x509_noanchor_init(x509_noanchor_context *xwc, const br_x509_class **inner) {
    xwc->vtable = &x509_noanchor_vtable;
    xwc->inner = inner;
}

static int sock_read(void *ctx, unsigned char *buf, size_t len) {
    for (;;) {
        ssize_t rlen = read(*(int *)ctx, buf, len);
        if (rlen <= 0) {
            if (rlen < 0 && errno == EINTR) continue;
            return -1;
        }
        return (int)rlen;
    }
}

static int sock_write(void *ctx, const unsigned char *buf, size_t len) {
    for (;;) {
        ssize_t wlen = write(*(int *)ctx, buf, len);
        if (wlen <= 0) {
            if (wlen < 0 && errno == EINTR) continue;
            return -1;
        }
        return (int)wlen;
    }
}

/* Connect TCP socket to host and port */
static int host_connect(const char *host, const char *port, int timeout_sec) {
    struct addrinfo hints, *si, *p;
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &si);
    if (err != 0) {
        return -1;
    }

    for (p = si; p != NULL; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        if (timeout_sec > 0) {
            struct timeval tv;
            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
        }

        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(si);
    return fd;
}

/* Parse standard URL */
static int parse_url(const char *url, bool *is_https, char *host, size_t max_host,
                     char *port, size_t max_port, char *path, size_t max_path) {
    if (!url) return -1;

    const char *p = url;
    if (strncmp(p, "https://", 8) == 0) {
        *is_https = true;
        p += 8;
        snprintf(port, max_port, "443");
    } else if (strncmp(p, "http://", 7) == 0) {
        *is_https = false;
        p += 7;
        snprintf(port, max_port, "80");
    } else {
        // Default to https
        *is_https = true;
        snprintf(port, max_port, "443");
    }

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = colon - p;
        if (hlen >= max_host) hlen = max_host - 1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';

        const char *port_start = colon + 1;
        size_t plen = slash ? (size_t)(slash - port_start) : strlen(port_start);
        if (plen >= max_port) plen = max_port - 1;
        strncpy(port, port_start, plen);
        port[plen] = '\0';
    } else {
        size_t hlen = slash ? (size_t)(slash - p) : strlen(p);
        if (hlen >= max_host) hlen = max_host - 1;
        strncpy(host, p, hlen);
        host[hlen] = '\0';
    }

    if (slash) {
        snprintf(path, max_path, "%s", slash);
    } else {
        snprintf(path, max_path, "/");
    }

    return 0;
}

static void print_usage(const char *prog) {
    printf("JurkOS wget (BearSSL Bare-Metal Edition)\n");
    printf("Usage: %s [-q] [-T timeout] [-O output_file] <URL>\n", prog);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    bool quiet = false;
    int timeout_sec = 10;
    const char *output_file = NULL;
    const char *url = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-q") == 0) {
            quiet = true;
        } else if (strcmp(argv[i], "-O") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            timeout_sec = atoi(argv[++i]);
        } else if (argv[i][0] != '-') {
            url = argv[i];
        }
    }

    if (!url) {
        print_usage(argv[0]);
        return 1;
    }

    bool is_https = true;
    char host[256] = {0};
    char port[16] = {0};
    char path[1024] = {0};

    if (parse_url(url, &is_https, host, sizeof(host), port, sizeof(port), path, sizeof(path)) != 0) {
        if (!quiet) fprintf(stderr, "wget: invalid URL: %s\n", url);
        return 1;
    }

    char default_out[256] = "download.bin";
    if (!output_file) {
        const char *last_slash = strrchr(path, '/');
        if (last_slash && strlen(last_slash + 1) > 0) {
            output_file = last_slash + 1;
        } else {
            output_file = default_out;
        }
    }

    if (!quiet) {
        printf("Connecting to %s (%s:%s)...\n", host, is_https ? "https" : "http", port);
    }

    int fd = host_connect(host, port, timeout_sec);
    if (fd < 0) {
        if (!quiet) fprintf(stderr, "wget: failed to connect to %s:%s\n", host, port);
        return 2;
    }

    FILE *out = fopen(output_file, "wb");
    if (!out) {
        if (!quiet) fprintf(stderr, "wget: cannot open output file %s: %s\n", output_file, strerror(errno));
        close(fd);
        return 3;
    }

    char req[2048];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: JurkOS-Wget-BearSSL/1.1\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n\r\n",
             path, host);

    size_t total_bytes = 0;

    if (!is_https) {
        // Plain HTTP
        (void)write(fd, req, strlen(req));

        char buf[4096];
        bool header_passed = false;
        int n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            if (!header_passed) {
                char *hdr_end = strstr(buf, "\r\n\r\n");
                if (hdr_end) {
                    header_passed = true;
                    char *body_start = hdr_end + 4;
                    int body_len = n - (body_start - buf);
                    if (body_len > 0) {
                        fwrite(body_start, 1, body_len, out);
                        total_bytes += body_len;
                    }
                }
            } else {
                fwrite(buf, 1, n, out);
                total_bytes += n;
            }
        }
        close(fd);
    } else {
        // BearSSL HTTPS
        br_ssl_client_context sc;
        br_x509_minimal_context xc;
        x509_noanchor_context xwc;
        unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
        br_sslio_context ioc;

        // Initialize BearSSL client with all supported ciphers
        br_ssl_client_init_full(&sc, &xc, NULL, 0);

        // Attach permissive x509 wrapper so embedded systems can download from GitHub
        x509_noanchor_init(&xwc, &xc.vtable);
        br_ssl_engine_set_x509(&sc.eng, &xwc.vtable);

        br_ssl_engine_set_buffer(&sc.eng, iobuf, sizeof(iobuf), 1);
        br_ssl_client_reset(&sc, host, 0);
        br_sslio_init(&ioc, &sc.eng, sock_read, &fd, sock_write, &fd);

        br_sslio_write_all(&ioc, req, strlen(req));
        br_sslio_flush(&ioc);

        char buf[4096];
        bool header_passed = false;

        for (;;) {
            int rlen = br_sslio_read(&ioc, (unsigned char*)buf, sizeof(buf));
            if (rlen <= 0) break;

            if (!header_passed) {
                char *hdr_end = strstr(buf, "\r\n\r\n");
                if (hdr_end) {
                    header_passed = true;
                    char *body_start = hdr_end + 4;
                    int body_len = rlen - (body_start - buf);
                    if (body_len > 0) {
                        fwrite(body_start, 1, body_len, out);
                        total_bytes += body_len;
                    }
                }
            } else {
                fwrite(buf, 1, rlen, out);
                total_bytes += rlen;
            }
        }

        close(fd);
    }

    fclose(out);

    if (total_bytes == 0) {
        if (!quiet) fprintf(stderr, "wget: downloaded 0 bytes from %s\n", url);
        unlink(output_file);
        return 4;
    }

    if (!quiet) {
        printf("[✓] Successfully saved to '%s' (%zu bytes)\n", output_file, total_bytes);
    }

    return 0;
}
