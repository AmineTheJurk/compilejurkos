/* Copyright (c) 2026 AmineTheJurk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/wait.h>

#define SETUP_MARKER "/encrypted/.setup_done"
#define USER_DB_PATH "/encrypted/userlogin/"

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *base64_encode(const char *data) {
    size_t input_len = strlen(data);
    size_t output_len = 4 * ((input_len + 2) / 3);
    char *encoded_data = malloc(output_len + 1);
    if (encoded_data == NULL) return NULL;
    for (size_t i = 0, j = 0; i < input_len;) {
        uint32_t octet_a = i < input_len ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_len ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_len ? (unsigned char)data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = (i > input_len + 1) ? '=' : base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = (i > input_len) ? '=' : base64_table[triple & 0x3F];
    }
    encoded_data[output_len] = '\0';
    return encoded_data;
}

static void make_dir_executable(const char *dir_path) {
    DIR *d = opendir(dir_path);
    if (!d) return;
    struct dirent *de;
    char path[512];
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", dir_path, de->d_name);
        (void)chmod(path, 0755);
    }
    closedir(d);
}

void setup_system(void) {
    (void)mount(NULL, "/", NULL, MS_REMOUNT, NULL);
    (void)mkdir("/proc", 0755); (void)mount("proc", "/proc", "proc", 0, NULL);
    (void)mkdir("/sys", 0755); (void)mount("sysfs", "/sys", "sysfs", 0, NULL);
    (void)mkdir("/dev", 0755); (void)mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    /* Ensure standard I/O descriptors 0, 1, 2 are attached to active console */
    int cons = open("/dev/console", O_RDWR);
    if (cons < 0) {
        cons = open("/dev/tty0", O_RDWR);
    }
    if (cons >= 0) {
        dup2(cons, 0);
        dup2(cons, 1);
        dup2(cons, 2);
        if (cons > 2) close(cons);
    }

    setlinebuf(stdout);
    setlinebuf(stderr);

    (void)mkdir("/etc", 0755); (void)symlink("/proc/mounts", "/etc/mtab");
    (void)mkdir("/usr", 0755); (void)mkdir("/usr/apps", 0755);
    (void)setenv("PATH", "/bin:/sbin:/usr/bin:/usr/apps", 1);
    (void)setenv("TERM", "linux", 1);
    (void)setenv("LS_COLORS", "none", 1);

    /* Guarantee execute permissions on all binaries at boot */
    make_dir_executable("/bin");
    make_dir_executable("/sbin");
    make_dir_executable("/usr/bin");

    /* Ensure /bin/sh symlink exists and points to /bin/busybox */
    if (access("/bin/sh", F_OK) != 0 && access("/bin/busybox", F_OK) == 0) {
        (void)symlink("/bin/busybox", "/bin/sh");
    }

    /* Ensure module utilities exist and point to busybox */
    const char *mod_tools[] = {"modprobe", "insmod", "rmmod", "lsmod", "depmod"};
    char tool_path[128];
    for (size_t i = 0; i < sizeof(mod_tools)/sizeof(mod_tools[0]); i++) {
        snprintf(tool_path, sizeof(tool_path), "/bin/%s", mod_tools[i]);
        if (access(tool_path, F_OK) != 0 && access("/bin/busybox", F_OK) == 0) {
            (void)symlink("/bin/busybox", tool_path);
        }
        snprintf(tool_path, sizeof(tool_path), "/sbin/%s", mod_tools[i]);
        if (access(tool_path, F_OK) != 0 && access("/bin/busybox", F_OK) == 0) {
            (void)symlink("/bin/busybox", tool_path);
        }
    }

    (void)system("busybox mdev -s");
}

void expand_filesystem(void) {
    printf("[JurkOS] Expanding storage...\n");
    (void)system("resize2fs /dev/disk/by-partuuid/12345678-01");
}

void strip_newline(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}

void run_shell(const char *username) {
    char home_dir[256];
    snprintf(home_dir, sizeof(home_dir), "/home/%s", username);
    (void)mkdir("/home", 0755);
    (void)mkdir(home_dir, 0755);
    if (chdir(home_dir) != 0) {
        /* Fallback to root if home dir fails */
        (void)chdir("/");
    }
    (void)setenv("HOME", home_dir, 1);

    char input[1024];
    printf("\n--- JurkOS Shell ---\n");

    while (1) {
        char cwd[256];
        if (!getcwd(cwd, sizeof(cwd))) {
            strncpy(cwd, "~", sizeof(cwd) - 1);
            cwd[sizeof(cwd) - 1] = '\0';
        }
        printf("jurkos@%s:%s$ ", username, cwd);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        strip_newline(input);
        if (strlen(input) == 0 || input[0] == '\033') continue;

        // C-Shell Built-ins
        if (strcmp(input, "help") == 0) {
            printf("Built-ins: help, wifi, JurkStore, opengl, fbgfx, sysinfo, whoami, clear, logout, reboot, shutdown, busybox, cd, exit\n");
            printf("  wifi       - Scan nearby Wi-Fi networks and connect (Open or WPA/WPA2)\n");
            printf("  JurkStore  - Launch the JurkStore TUI App Store\n");
            printf("  opengl     - Launch 3D Hardware-Accelerated OpenGL on DRM/KMS demo\n");
            printf("  fbgfx      - Launch Linux framebuffer animated graphics demo\n");
            printf("  busybox    - Drop into BusyBox interactive shell\n");
            printf("  cd <dir>   - Change current working directory\n");
            printf("  sysinfo    - Display JurkOS version and kernel info\n");
            printf("  shutdown   - Power off the system\n");
            printf("  reboot     - Reboot the system\n");
            continue;
        }
        if (strcmp(input, "clear") == 0) { printf("\033[H\033[J"); continue; }
        if (strcmp(input, "logout") == 0 || strcmp(input, "exit") == 0) break;
        if (strcmp(input, "reboot") == 0) { sync(); (void)system("reboot"); continue; }
        if (strcmp(input, "shutdown") == 0 || strcmp(input, "poweroff") == 0) { sync(); (void)system("poweroff"); continue; }
        if (strcmp(input, "sysinfo") == 0) { printf("OS: JurkOS\nKernel: Linux\n"); continue; }
        if (strcmp(input, "whoami") == 0) { printf("%s\n", username); continue; }

        // If input contains shell operators (redirection >, <, pipes |, chains &&, ||, ;)
        // execute through BusyBox shell so redirection and pipes work as expected
        if (strpbrk(input, "><|;&*?") != NULL) {
            pid_t pid = fork();
            if (pid == 0) {
                char *sh_args[] = { "sh", "-c", input, NULL };
                execve("/bin/busybox", sh_args, NULL);
                execve("/bin/sh", sh_args, NULL);
                // Fallback to system()
                int ret = system(input);
                exit(ret);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // Tokenize input into arguments: args[0], args[1], ...
        char input_copy[1024];
        strncpy(input_copy, input, sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        char *args[64];
        int argc_cmd = 0;
        char *p = input_copy;
        while (*p) {
            while (*p == ' ' || *p == '\t') *p++ = '\0';
            if (*p == '\0') break;
            if (argc_cmd < 63) args[argc_cmd++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        args[argc_cmd] = NULL;
        if (argc_cmd == 0) continue;

        char *cmd = args[0];
        char *env[] = {
            "PATH=/bin:/sbin:/usr/bin:/usr/apps",
            "TERM=linux",
            "USER=jurkos",
            "SHELL=/bin/busybox",
            NULL
        };

        // Built-in 'cd': change working directory with chdir
        if (strcmp(cmd, "cd") == 0) {
            char *path = (argc_cmd > 1) ? args[1] : home_dir;
            if (strcmp(path, "~") == 0) path = home_dir;
            if (chdir(path) != 0) {
                printf("cd: no such directory: %s\n", path);
            }
            continue;
        }

        // Built-in 'wifi': Scan and connect to wireless networks
        if (strcmp(cmd, "wifi") == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                execve("/bin/wifi", args, env);
                execve("/sbin/wifi", args, env);
                execve("/iso/bin/wifi", args, env);
                perror("wifi");
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // Built-in 'JurkStore': run store binary directly via fork+execve
        if (strcmp(cmd, "JurkStore") == 0 || strcmp(cmd, "jurkstore") == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                char *js_args[] = { "JurkStore", NULL };
                execve("/bin/JurkStore", js_args, env);
                execve("/iso/bin/JurkStore", js_args, env);
                perror("JurkStore");
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // Built-in 'busybox' interactive shell: argv[0] MUST be "sh" or "ash"
        if ((strcmp(cmd, "busybox") == 0 && argc_cmd == 1) || strcmp(cmd, "sh") == 0 || strcmp(cmd, "ash") == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                char *sh_args[] = { "sh", NULL };
                execve("/bin/busybox", sh_args, env);
                execve("/bin/sh", sh_args, env);
                perror("Failed to start BusyBox shell");
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // If user ran 'busybox <applet> [args...]'
        if (strcmp(cmd, "busybox") == 0 && argc_cmd > 1) {
            pid_t pid = fork();
            if (pid == 0) {
                execve("/bin/busybox", args, env);
                perror("busybox");
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // 1. Check installed JurkStore apps (/usr/apps/<cmd>/<cmd>)
        char app_exec[512];
        snprintf(app_exec, sizeof(app_exec), "/usr/apps/%s/%s", cmd, cmd);
        if (access(app_exec, X_OK) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                execve(app_exec, args, env);
                perror(cmd);
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // 2. Check standard system binary paths (/bin, /sbin, /usr/bin)
        char bin_path[512];
        int found = 0;
        const char *search_dirs[] = {"/bin", "/sbin", "/usr/bin", NULL};
        for (int d = 0; search_dirs[d] != NULL; d++) {
            snprintf(bin_path, sizeof(bin_path), "%s/%s", search_dirs[d], cmd);
            if (access(bin_path, X_OK) == 0) {
                found = 1;
                break;
            }
        }

        if (found) {
            pid_t pid = fork();
            if (pid == 0) {
                execve(bin_path, args, env);
                // If it was a busybox applet binary that needs fallback
                execve("/bin/busybox", args, env);
                perror(cmd);
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                continue;
            }
        }

        // 3. Fallback: Run directly through BusyBox multi-call applets (e.g. ls, cat, echo, ps)
        if (access("/bin/busybox", X_OK) == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                // Try executing with argv[0] = cmd
                execve("/bin/busybox", args, env);

                // Or executing as /bin/busybox <cmd> [args...]
                char *bb_args[66];
                bb_args[0] = "busybox";
                for (int i = 0; i < argc_cmd && i < 63; i++) {
                    bb_args[i + 1] = args[i];
                }
                bb_args[argc_cmd + 1] = NULL;
                execve("/bin/busybox", bb_args, env);
                exit(127);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) != 127) {
                    continue;
                }
            }
        }

        printf("No Such Command, Did you misstype?\n");
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    setup_system();
    FILE *rw_check = fopen("/.rw_test", "w");
    if (!rw_check) {
        printf("\nUSB is read-only! Check flash mode.\n");
        while(1) { (void)sleep(100); }
    }
    fclose(rw_check);
    (void)remove("/.rw_test");

    struct stat st = {0};
    if (stat(SETUP_MARKER, &st) == -1) {
        printf("What would you like to call you?\n");
        char username[64] = "AmineTheJurk", password[64] = "";
        while (1) {
            printf("Username : ");
            if (fgets(username, sizeof(username), stdin)) {
                strip_newline(username);
                if (strlen(username) > 0) break;
            }
            sleep(1);
        }
        printf("Password : ");
        if (fgets(password, sizeof(password), stdin)) {
            strip_newline(password);
        }

        expand_filesystem();
        (void)mkdir("/encrypted", 0755);
        (void)mkdir(USER_DB_PATH, 0755);
        char user_path[256]; snprintf(user_path, sizeof(user_path), "%s%s", USER_DB_PATH, username);
        (void)mkdir(user_path, 0755);
        char creds[128], auth_file[512];
        snprintf(creds, sizeof(creds), "%s:%s", username, password);
        char *encoded = base64_encode(creds);
        snprintf(auth_file, sizeof(auth_file), "%s/auth", user_path);
        FILE *f = fopen(auth_file, "w");
        if (f) { fprintf(f, "%s", encoded); fclose(f); }
        free(encoded);
        FILE *marker = fopen(SETUP_MARKER, "w");
        if (marker) { fprintf(marker, "%s", username); fclose(marker); }
        while (1) {
            run_shell(username);
        }
    } else {
        char username[64] = "AmineTheJurk";
        FILE *marker = fopen(SETUP_MARKER, "r");
        if (marker) {
            if (fgets(username, sizeof(username), marker)) {
                strip_newline(username);
            }
            fclose(marker);
        }
        if (strlen(username) == 0) strncpy(username, "AmineTheJurk", sizeof(username));

        while (1) {
            printf("\nPassword? : ");
            char password[64];
            if (!fgets(password, sizeof(password), stdin)) {
                sleep(1);
                continue;
            }
            strip_newline(password);
            if (strlen(password) == 0) { 
                run_shell(username); 
                continue; 
            }

            char user_path[256], auth_file[512];
            snprintf(user_path, sizeof(user_path), "%s%s", USER_DB_PATH, username);
            snprintf(auth_file, sizeof(auth_file), "%s/auth", user_path);
            FILE *f = fopen(auth_file, "r");
            if (!f) {
                // If user auth file is missing, drop to shell rather than panicking
                printf("Warning: Auth profile not found. Starting emergency shell.\n");
                run_shell(username);
                continue;
            }
            char stored_encoded[256];
            if (!fgets(stored_encoded, sizeof(stored_encoded), f)) {
                stored_encoded[0] = '\0';
            }
            fclose(f);
            char input_creds[128];
            snprintf(input_creds, sizeof(input_creds), "%s:%s", username, password);
            char *input_encoded = base64_encode(input_creds);
            if (input_encoded && strcmp(stored_encoded, input_encoded) == 0) {
                free(input_encoded);
                run_shell(username);
            } else {
                printf("Incorrect password.\n"); 
                free(input_encoded);
            }
        }
    }
    while (1) { sleep(100); }
    return 0;
}
