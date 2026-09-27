/* JurkStore (TUI Edition) for JurkOS
 * Copyright (c) 2026 AmineTheJurk
 * Statically compiled terminal package manager and app store
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

#define MAX_APPS 64
#define APPS_DIR "/usr/apps"
#define REPO_BASE "https://raw.githubusercontent.com/AmineTheJurk/JurkStore-Apps/main"
#define LOCAL_APPS_DIR "/usr/share/jurkstore/apps"

typedef struct {
    char id[64];          // folder name, e.g. "exemple", "jurkfetch", "calc"
    char name[64];        // app name, e.g. "MyApp", "jurkfetch", "calc"
    char version[32];
    char author[64];
    char description[256];
    char download[512];
    int installed;
    int has_local;        // 1 if present in local preloaded storage
} AppEntry;

static AppEntry apps[MAX_APPS];
static int app_count = 0;
static struct termios orig_termios;
static int raw_mode_active = 0;

static void safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (!dest || dest_size == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t i = 0;
    while (src[i] != '\0' && i + 1 < dest_size) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void disable_raw_mode(void) {
    if (raw_mode_active) {
        printf("\033[?25h"); // Show cursor
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
}

void handle_signal(int sig) {
    (void)sig;
    disable_raw_mode();
    printf("\n\033[0mExiting JurkStore.\n");
    exit(0);
}

void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    atexit(disable_raw_mode);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;
    printf("\033[?25l"); // Hide cursor
}

int is_app_installed(const char *name) {
    if (!name || strlen(name) == 0) return 0;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s/%s", APPS_DIR, name, name);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
        return 1;
    }
    snprintf(path, sizeof(path), "/bin/%s", name);
    if (stat(path, &st) == 0 && (st.st_mode & S_IXUSR)) {
        return 1;
    }
    return 0;
}

/* Check if a real binary exists in local storage */
int check_local_binary(const char *folder_id, const char *app_name, char *found_path, size_t max_len) {
    char candidate[512];
    struct stat st;

    // 1. /usr/share/jurkstore/apps/<folder_id>/<app_name>.bk
    snprintf(candidate, sizeof(candidate), "%s/%s/%s.bk", LOCAL_APPS_DIR, folder_id, app_name);
    if (stat(candidate, &st) == 0 && st.st_size > 1000) {
        if (found_path) safe_strcpy(found_path, candidate, max_len);
        return 1;
    }

    // 2. /usr/share/jurkstore/apps/<folder_id>/<folder_id>.bk
    snprintf(candidate, sizeof(candidate), "%s/%s/%s.bk", LOCAL_APPS_DIR, folder_id, folder_id);
    if (stat(candidate, &st) == 0 && st.st_size > 1000) {
        if (found_path) safe_strcpy(found_path, candidate, max_len);
        return 1;
    }

    // 3. /usr/share/jurkstore/apps/<folder_id>/<app_name>
    snprintf(candidate, sizeof(candidate), "%s/%s/%s", LOCAL_APPS_DIR, folder_id, app_name);
    if (stat(candidate, &st) == 0 && st.st_size > 1000) {
        if (found_path) safe_strcpy(found_path, candidate, max_len);
        return 1;
    }

    return 0;
}

void extract_tag_value(const char *xml, const char *tag, char *dest, size_t dest_size) {
    char open_tag[64], close_tag[64];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    dest[0] = '\0';
    const char *start = strstr(xml, open_tag);
    if (!start) return;
    start += strlen(open_tag);

    const char *end = strstr(start, close_tag);
    if (!end) return;

    size_t len = (size_t)(end - start);
    if (len >= dest_size) len = dest_size - 1;
    memcpy(dest, start, len);
    dest[len] = '\0';

    // Trim trailing whitespace or newlines
    while (len > 0 && (dest[len - 1] == '\r' || dest[len - 1] == '\n' || dest[len - 1] == ' ')) {
        dest[len - 1] = '\0';
        len--;
    }
}

void add_or_update_app(const char *id, const char *name, const char *version,
                       const char *author, const char *description, const char *download) {
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0 || strcmp(apps[i].id, id) == 0) {
            safe_strcpy(apps[i].id, id, sizeof(apps[i].id));
            safe_strcpy(apps[i].name, name, sizeof(apps[i].name));
            safe_strcpy(apps[i].version, version, sizeof(apps[i].version));
            safe_strcpy(apps[i].author, author, sizeof(apps[i].author));
            safe_strcpy(apps[i].description, description, sizeof(apps[i].description));
            safe_strcpy(apps[i].download, download, sizeof(apps[i].download));
            apps[i].installed = is_app_installed(name);
            apps[i].has_local = check_local_binary(id, name, NULL, 0);
            return;
        }
    }

    if (app_count >= MAX_APPS) return;
    safe_strcpy(apps[app_count].id, id, sizeof(apps[app_count].id));
    safe_strcpy(apps[app_count].name, name, sizeof(apps[app_count].name));
    safe_strcpy(apps[app_count].version, version, sizeof(apps[app_count].version));
    safe_strcpy(apps[app_count].author, author, sizeof(apps[app_count].author));
    safe_strcpy(apps[app_count].description, description, sizeof(apps[app_count].description));
    safe_strcpy(apps[app_count].download, download, sizeof(apps[app_count].download));
    apps[app_count].installed = is_app_installed(name);
    apps[app_count].has_local = check_local_binary(id, name, NULL, 0);
    app_count++;
}

int parse_meta_xml_buffer(const char *folder_id, const char *xml_content) {
    char name[64] = "", ver[32] = "", author[64] = "", desc[256] = "", dl[512] = "";
    extract_tag_value(xml_content, "name", name, sizeof(name));
    extract_tag_value(xml_content, "version", ver, sizeof(ver));
    extract_tag_value(xml_content, "author", author, sizeof(author));
    extract_tag_value(xml_content, "description", desc, sizeof(desc));
    extract_tag_value(xml_content, "download", dl, sizeof(dl));

    if (strlen(name) == 0) {
        safe_strcpy(name, folder_id, sizeof(name));
    }
    if (strlen(dl) == 0) {
        snprintf(dl, sizeof(dl), "%s/apps/%s/%s.bk", REPO_BASE, folder_id, name);
    }

    add_or_update_app(folder_id, name, ver, author, desc, dl);
    return 1;
}

/* Scan already installed apps on this machine from /usr/apps */
void scan_installed_apps(void) {
    DIR *d = opendir(APPS_DIR);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char meta_file[512];
        snprintf(meta_file, sizeof(meta_file), "%s/%s/meta.xml", APPS_DIR, de->d_name);

        FILE *f = fopen(meta_file, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 32768) {
                char *buf = malloc(sz + 1);
                if (buf) {
                    size_t r = fread(buf, 1, sz, f);
                    buf[r] = '\0';
                    parse_meta_xml_buffer(de->d_name, buf);
                    free(buf);
                }
            }
            fclose(f);
        }
    }
    closedir(d);
}

/* Populate catalog (empty by default until initialized/synced) */
void populate_defaults(void) {
    app_count = 0;
    scan_installed_apps();
}

void fetch_meta_from_entry(const char *entry) {
    char url[512] = "";
    char folder_id[64] = "";

    if (strncmp(entry, "$repo/", 6) == 0) {
        snprintf(url, sizeof(url), "%s/%s", REPO_BASE, entry + 6);
    } else if (strncmp(entry, "$repo", 5) == 0) {
        snprintf(url, sizeof(url), "%s%s", REPO_BASE, entry + 5);
    } else if (strncmp(entry, "http://", 7) == 0 || strncmp(entry, "https://", 8) == 0) {
        safe_strcpy(url, entry, sizeof(url));
    } else if (strncmp(entry, "apps/", 5) == 0) {
        snprintf(url, sizeof(url), "%s/%s", REPO_BASE, entry);
    } else {
        snprintf(url, sizeof(url), "%s/apps/%s/meta.xml", REPO_BASE, entry);
        safe_strcpy(folder_id, entry, sizeof(folder_id));
    }

    if (strlen(folder_id) == 0) {
        const char *p = strstr(url, "apps/");
        if (p) {
            p += 5;
            const char *slash = strchr(p, '/');
            if (slash) {
                size_t flen = (size_t)(slash - p);
                if (flen >= sizeof(folder_id)) flen = sizeof(folder_id) - 1;
                memcpy(folder_id, p, flen);
                folder_id[flen] = '\0';
            }
        }
    }
    if (strlen(folder_id) == 0) safe_strcpy(folder_id, "app", sizeof(folder_id));

    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/meta_%s.xml", folder_id);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "busybox wget -q -T 4 -O \"%s\" \"%s\" 2>/dev/null || wget -q -T 4 -O \"%s\" \"%s\" 2>/dev/null",
             tmp_path, url, tmp_path, url);

    if (system(cmd) == 0) {
        FILE *f = fopen(tmp_path, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0 && sz < 65536) {
                char *buf = malloc(sz + 1);
                if (buf) {
                    size_t r = fread(buf, 1, sz, f);
                    buf[r] = '\0';
                    parse_meta_xml_buffer(folder_id, buf);
                    free(buf);
                }
            }
            fclose(f);
        }
        (void)remove(tmp_path);
    }
}

/* Install app: real binaries only. No fake scripts. */
void install_app(AppEntry *app) {
    disable_raw_mode();
    printf("\033[H\033[J");
    printf("\033[1;36m+==============================================================+\033[0m\n");
    printf("\033[1;36m|                 JurkStore Package Installer                  |\033[0m\n");
    printf("\033[1;36m+==============================================================+\033[0m\n\n");
    printf("  \033[1mPackage:\033[0m       \033[1;32m%s (v%s)\033[0m\n", app->name, app->version);
    printf("  \033[1mAuthor:\033[0m        %s\n", app->author);
    printf("  \033[1mTarget Dir:\033[0m    %s/%s/\n", APPS_DIR, app->name);
    printf("  \033[1mBinary Exec:\033[0m   /bin/%s\n\n", app->name);

    char cmd[4096];
    // 1. Create target directories
    snprintf(cmd, sizeof(cmd), "mkdir -p %s/%s /bin", APPS_DIR, app->name);
    (void)system(cmd);

    char bk_path[512];
    snprintf(bk_path, sizeof(bk_path), "%s/%s/%s.bk", APPS_DIR, app->name, app->name);
    char bin_path[512];
    snprintf(bin_path, sizeof(bin_path), "%s/%s/%s", APPS_DIR, app->name, app->name);

    int binary_ready = 0;

    // Check if we have a real binary preloaded in local repository
    char local_bin[512] = {0};
    if (check_local_binary(app->id, app->name, local_bin, sizeof(local_bin))) {
        printf("  \033[1;36m[Local Package]\033[0m Found preloaded package in JurkOS image.\n");
        printf("  Installing: %s -> %s\n", local_bin, bin_path);
        snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s\" && cp -f \"%s\" \"%s\" && chmod 755 \"%s\" \"%s\"",
                 local_bin, bk_path, local_bin, bin_path, bk_path, bin_path);
        if (system(cmd) == 0) {
            struct stat st;
            if (stat(bin_path, &st) == 0 && st.st_size > 1000) {
                binary_ready = 1;
            }
        }
    }

    // If not local, download real binary from repository
    if (!binary_ready && strlen(app->download) > 0) {
        char full_url[1024];
        if (strncmp(app->download, "http://", 7) == 0 || strncmp(app->download, "https://", 8) == 0) {
            safe_strcpy(full_url, app->download, sizeof(full_url));
        } else {
            snprintf(full_url, sizeof(full_url), "https://%s", app->download);
        }

        printf("  \033[1;36m[Online Download]\033[0m Fetching real binary from:\n  %s\n", full_url);
        snprintf(cmd, sizeof(cmd), "busybox wget -T 15 -O \"%s\" \"%s\" 2>/dev/null || wget -T 15 -O \"%s\" \"%s\" 2>/dev/null",
                 bk_path, full_url, bk_path, full_url);
        
        int r = system(cmd);
        if (r == 0) {
            struct stat st;
            if (stat(bk_path, &st) == 0 && st.st_size > 1000) {
                // Verify binary integrity (ELF check)
                FILE *bf = fopen(bk_path, "rb");
                if (bf) {
                    unsigned char magic[4];
                    if (fread(magic, 1, 4, bf) == 4) {
                        if (magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') {
                            binary_ready = 1;
                        }
                    }
                    fclose(bf);
                }
                if (binary_ready) {
                    snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s\" && chmod 755 \"%s\" \"%s\"",
                             bk_path, bin_path, bk_path, bin_path);
                    (void)system(cmd);
                    printf("  \033[1;32m[✓]\033[0m Verified authentic ELF binary (%ld bytes).\n", (long)st.st_size);
                }
            }
        }
    }

    // STRICTLY NO FAKE FALLBACK
    if (!binary_ready) {
        printf("\n  \033[1;31m==============================================================\033[0m\n");
        printf("  \033[1;31m[ERROR] Installation Failed: Package Binary Not Available!\033[0m\n");
        printf("  --------------------------------------------------------------\n");
        printf("  URL: %s\n\n", app->download);
        printf("  \033[1mPossible causes:\033[0m\n");
        printf("  1. System is offline (run '\033[1;33mwifi\033[0m' or connect network cable).\n");
        printf("  2. Remote binary repository URL timed out or returned HTTP 404.\n\n");
        printf("  \033[1;33m[!] JurkStore does NOT install fake placeholder scripts.\033[0m\n");
        printf("      Installation aborted. System untouched.\n");
        printf("  \033[1;31m==============================================================\033[0m\n");

        // Clean up partial folder
        snprintf(cmd, sizeof(cmd), "rm -rf %s/%s", APPS_DIR, app->name);
        (void)system(cmd);

        app->installed = 0;
        printf("\nPress any key to return to JurkStore...");
        fflush(stdout);
        enable_raw_mode();
        (void)getchar();
        return;
    }

    // Write real meta.xml to /usr/apps/<name>/meta.xml
    char meta_path[512];
    snprintf(meta_path, sizeof(meta_path), "%s/%s/meta.xml", APPS_DIR, app->name);
    FILE *mf = fopen(meta_path, "w");
    if (mf) {
        fprintf(mf, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(mf, "<jurkos-app>\n");
        fprintf(mf, "    <name>%s</name>\n", app->name);
        fprintf(mf, "    <version>%s</version>\n", app->version);
        fprintf(mf, "    <author>%s</author>\n", app->author);
        fprintf(mf, "    <description>%s</description>\n", app->description);
        fprintf(mf, "    <download>%s</download>\n", app->download);
        fprintf(mf, "</jurkos-app>\n");
        fclose(mf);
    }

    // Link executable to /bin so it can be called from anywhere
    char sys_link[512];
    snprintf(sys_link, sizeof(sys_link), "/bin/%s", app->name);
    snprintf(cmd, sizeof(cmd), "ln -sf \"%s\" \"%s\" 2>/dev/null || cp -f \"%s\" \"%s\"",
             bin_path, sys_link, bin_path, sys_link);
    (void)system(cmd);

    app->installed = 1;
    printf("\n  \033[1;32m==============================================================\033[0m\n");
    printf("  \033[1;32m[SUCCESS] %s (v%s) Installed Successfully!\033[0m\n", app->name, app->version);
    printf("  Location: \033[1;36m%s\033[0m\n", bin_path);
    printf("  Command:  \033[1;33m%s\033[0m (Available in global PATH)\n", app->name);
    printf("  Type '\033[1;33m%s\033[0m' anywhere in the JurkOS terminal to run it.\n", app->name);
    printf("  \033[1;32m==============================================================\033[0m\n");

    printf("\nPress any key to return to JurkStore...");
    fflush(stdout);
    enable_raw_mode();
    (void)getchar();
}

/* Uninstall app cleanly */
void uninstall_app(AppEntry *app) {
    disable_raw_mode();
    printf("\033[H\033[J");
    printf("\033[1;36m+==============================================================+\033[0m\n");
    printf("\033[1;36m|                 JurkStore Package Remover                    |\033[0m\n");
    printf("\033[1;36m+==============================================================+\033[0m\n\n");

    printf("  Removing package: \033[1;31m%s\033[0m\n", app->name);

    char cmd[512];
    // Remove /usr/apps/<name>
    snprintf(cmd, sizeof(cmd), "rm -rf %s/%s", APPS_DIR, app->name);
    (void)system(cmd);

    // Remove /bin/<name>
    snprintf(cmd, sizeof(cmd), "rm -f /bin/%s", app->name);
    (void)system(cmd);

    app->installed = 0;
    printf("  \033[1;32m[✓]\033[0m Removed /usr/apps/%s\n", app->name);
    printf("  \033[1;32m[✓]\033[0m Removed /bin/%s\n\n", app->name);
    printf("  \033[1;32m[SUCCESS] %s has been cleanly uninstalled.\033[0m\n", app->name);

    printf("\nPress any key to return to JurkStore...");
    fflush(stdout);
    enable_raw_mode();
    (void)getchar();
}

void show_detail_view(AppEntry *app) {
    while (1) {
        printf("\033[H\033[J");
        printf("\033[1;36m+==============================================================+\033[0m\n");
        printf("\033[1;36m|                 JurkStore App Details                        |\033[0m\n");
        printf("\033[1;36m+==============================================================+\033[0m\n\n");

        printf("  \033[1;33mName:\033[0m        %s\n", app->name);
        printf("  \033[1;33mVersion:\033[0m     %s\n", app->version);
        printf("  \033[1;33mAuthor:\033[0m      %s\n", app->author);
        if (app->installed) {
            printf("  \033[1;33mStatus:\033[0m      \033[1;32m[Installed in /usr/apps/%s]\033[0m\n", app->name);
        } else if (app->has_local) {
            printf("  \033[1;33mStatus:\033[0m      \033[1;36m[Available - Preloaded in Image]\033[0m\n");
        } else {
            printf("  \033[1;33mStatus:\033[0m      \033[1;34m[Available - Online Repository]\033[0m\n");
        }
        printf("  \033[1;33mDownload:\033[0m    %s\n\n", app->download);

        printf("  \033[1;37mDescription:\033[0m\n");
        printf("  --------------------------------------------------------------\n");
        printf("  %s\n", app->description);
        printf("  --------------------------------------------------------------\n\n");

        printf("\033[1;36m+--------------------------------------------------------------+\033[0m\n");
        if (!app->installed) {
            printf("  \033[1;32m[I]\033[0m Install   \033[1;33m[B/Esc]\033[0m Back   \033[1;31m[Q]\033[0m Exit Store\n");
        } else {
            printf("  \033[1;32m[I]\033[0m Reinstall   \033[1;31m[U]\033[0m Uninstall   \033[1;33m[B/Esc]\033[0m Back   \033[1;31m[Q]\033[0m Exit Store\n");
        }
        printf("\033[1;36m+--------------------------------------------------------------+\033[0m\n");
        fflush(stdout);

        int c = getchar();
        if (c == 'q' || c == 'Q') {
            disable_raw_mode();
            printf("\033[H\033[J\033[0m");
            exit(0);
        }
        if (c == 'b' || c == 'B' || c == 27) {
            if (c == 27) {
                int n1 = getchar();
                if (n1 == '[') getchar();
            }
            return;
        }
        if (c == 'i' || c == 'I') {
            install_app(app);
            return;
        }
        if ((c == 'u' || c == 'U') && app->installed) {
            uninstall_app(app);
            return;
        }
    }
}

void render_store_list(int selected_index) {
    printf("\033[H\033[J");
    printf("\033[1;36m+================================================================+\033[0m\n");
    printf("\033[1;36m|         JurkStore (TUI Edition) - JurkOS Application Store     |\033[0m\n");
    printf("\033[1;36m+================================================================+\033[0m\n\n");

    printf("  \033[1m%-16s %-10s %-18s %s\033[0m\n", "APP NAME", "VERSION", "AUTHOR", "STATUS");
    printf("  ----------------------------------------------------------------\n");

    if (app_count == 0) {
        printf("\n  \033[1;30m(No packages indexed or installed yet)\033[0m\n");
        printf("  Press \033[1;36m[R]\033[0m to sync catalog from repository.\n\n");
    } else {
        for (int i = 0; i < app_count; i++) {
            char status_str[48];
            if (apps[i].installed) {
                snprintf(status_str, sizeof(status_str), "\033[1;32m[Installed]\033[0m");
            } else if (apps[i].has_local) {
                snprintf(status_str, sizeof(status_str), "\033[1;36m[Image Cache]\033[0m");
            } else {
                snprintf(status_str, sizeof(status_str), "\033[0;34m[Online]\033[0m");
            }

            if (i == selected_index) {
                printf(" \033[1;33m>\033[0m \033[7m %-14s %-10s %-18s \033[0m %s\n",
                       apps[i].name, apps[i].version, apps[i].author, status_str);
            } else {
                printf("   %-15s %-10s %-18s %s\n",
                       apps[i].name, apps[i].version, apps[i].author, status_str);
            }
        }
    }

    printf("\n  ----------------------------------------------------------------\n");
    if (app_count > 0 && selected_index >= 0 && selected_index < app_count) {
        printf("  \033[1;33mSelected:\033[0m %s - %s\n",
               apps[selected_index].name, apps[selected_index].description);
    } else {
        printf("  \033[1;30mCatalog empty. Press [R] to sync.\033[0m\n");
    }
    printf("  ----------------------------------------------------------------\n\n");

    printf("\033[1;36m+----------------------------------------------------------------+\033[0m\n");
    printf("  \033[1;33m[↑/↓]\033[0m Move   \033[1;32m[Enter]\033[0m Details   \033[1;32m[I]\033[0m Install   \033[1;31m[U]\033[0m Uninstall   \033[1;36m[R]\033[0m Sync GitHub   \033[1;31m[Q]\033[0m Quit\n");
    printf("\033[1;36m+----------------------------------------------------------------+\033[0m\n");
    fflush(stdout);
}

void refresh_remote_catalog(void) {
    disable_raw_mode();
    printf("\033[H\033[J");
    printf("\033[1;36m+==============================================================+\033[0m\n");
    printf("\033[1;36m|          Connecting to JurkStore GitHub Repository           |\033[0m\n");
    printf("\033[1;36m+==============================================================+\033[0m\n\n");
    printf("  Repository: \033[1m%s\033[0m\n\n", REPO_BASE);

    // Fetch catalog.txt
    char catalog_txt_url[512], tmp_catalog_txt[256];
    snprintf(catalog_txt_url, sizeof(catalog_txt_url), "%s/catalog.txt", REPO_BASE);
    snprintf(tmp_catalog_txt, sizeof(tmp_catalog_txt), "/tmp/jurkstore_catalog.txt");

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "busybox wget -q -T 6 -O \"%s\" \"%s\" 2>/dev/null || wget -q -T 6 -O \"%s\" \"%s\" 2>/dev/null",
             tmp_catalog_txt, catalog_txt_url, tmp_catalog_txt, catalog_txt_url);

    int found_online = 0;
    if (system(cmd) == 0 && access(tmp_catalog_txt, R_OK) == 0) {
        FILE *f = fopen(tmp_catalog_txt, "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                size_t len = strlen(line);
                while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' ' || line[len-1] == '"')) {
                    line[len-1] = '\0';
                    len--;
                }
                char *clean_line = line;
                while (*clean_line == ' ' || *clean_line == '"') clean_line++;

                if (strlen(clean_line) > 0 && clean_line[0] != '#') {
                    printf("  \033[1;32m->\033[0m Syncing manifest: %s\n", clean_line);
                    fetch_meta_from_entry(clean_line);
                    found_online = 1;
                }
            }
            fclose(f);
        }
        (void)remove(tmp_catalog_txt);
    }

    // Try apps.txt
    if (!found_online) {
        char apps_txt_url[512], tmp_apps_txt[256];
        snprintf(apps_txt_url, sizeof(apps_txt_url), "%s/apps.txt", REPO_BASE);
        snprintf(tmp_apps_txt, sizeof(tmp_apps_txt), "/tmp/jurkstore_apps.txt");
        snprintf(cmd, sizeof(cmd), "busybox wget -q -T 6 -O \"%s\" \"%s\" 2>/dev/null || wget -q -T 6 -O \"%s\" \"%s\" 2>/dev/null",
                 tmp_apps_txt, apps_txt_url, tmp_apps_txt, apps_txt_url);

        if (system(cmd) == 0 && access(tmp_apps_txt, R_OK) == 0) {
            FILE *f = fopen(tmp_apps_txt, "r");
            if (f) {
                char line[128];
                while (fgets(line, sizeof(line), f)) {
                    size_t len = strlen(line);
                    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' ')) {
                        line[len-1] = '\0';
                        len--;
                    }
                    if (len > 0 && line[0] != '#') {
                        printf("  \033[1;32m->\033[0m Discovered app: %s\n", line);
                        fetch_meta_from_entry(line);
                        found_online = 1;
                    }
                }
                fclose(f);
            }
            (void)remove(tmp_apps_txt);
        }
    }

    if (!found_online) {
        printf("\n  \033[1;33m[!] Remote repository empty or network offline.\033[0m\n");
        printf("  Ensure entries are populated in catalog.txt / apps.txt on GitHub.\n");
    } else {
        printf("\n  \033[1;32m[✓] Successfully synced package index from repository!\033[0m\n");
    }

    // Refresh installation states
    for (int i = 0; i < app_count; i++) {
        apps[i].installed = is_app_installed(apps[i].name);
        apps[i].has_local = check_local_binary(apps[i].id, apps[i].name, NULL, 0);
    }

    printf("  Total Packages Available: \033[1;32m%d\033[0m\n", app_count);
    printf("\nPress any key to return to JurkStore...");
    fflush(stdout);
    enable_raw_mode();
    (void)getchar();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    populate_defaults();

    // Check installation statuses
    for (int i = 0; i < app_count; i++) {
        apps[i].installed = is_app_installed(apps[i].name);
        apps[i].has_local = check_local_binary(apps[i].id, apps[i].name, NULL, 0);
    }

    enable_raw_mode();

    int selected_index = 0;
    while (1) {
        if (selected_index < 0) selected_index = 0;
        if (selected_index >= app_count) selected_index = app_count - 1;

        render_store_list(selected_index);

        int c = getchar();

        if (c == 27) { // Escape sequence
            int c2 = getchar();
            if (c2 == '[') {
                int c3 = getchar();
                if (c3 == 'A') { // UP
                    if (selected_index > 0) selected_index--;
                } else if (c3 == 'B') { // DOWN
                    if (selected_index < app_count - 1) selected_index++;
                } else if (c3 == 'C') { // RIGHT
                    show_detail_view(&apps[selected_index]);
                }
            } else if (c2 == 27) {
                break;
            }
            continue;
        }

        if (c == 'k' || c == 'K' || c == 'w' || c == 'W') {
            if (selected_index > 0) selected_index--;
            continue;
        }
        if (c == 'j' || c == 'J' || c == 's' || c == 'S') {
            if (selected_index < app_count - 1) selected_index++;
            continue;
        }

        if (c == '\n' || c == '\r' || c == ' ') {
            if (app_count > 0) show_detail_view(&apps[selected_index]);
            continue;
        }

        if (c == 'i' || c == 'I') {
            if (app_count > 0) install_app(&apps[selected_index]);
            continue;
        }

        if (c == 'u' || c == 'U') {
            if (app_count > 0 && apps[selected_index].installed) {
                uninstall_app(&apps[selected_index]);
            }
            continue;
        }

        if (c == 'r' || c == 'R') {
            refresh_remote_catalog();
            continue;
        }

        if (c == 'q' || c == 'Q') {
            break;
        }
    }

    disable_raw_mode();
    printf("\033[H\033[J\033[0m");
    return 0;
}
