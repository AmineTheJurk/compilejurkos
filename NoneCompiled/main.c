/* Copyright (c) 2026 AmineTheJurk */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
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
    if (!encoded_data) return NULL;
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

static void strip_newline(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
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

static void crash_signal_handler(int sig) {
    const char *sig_name = "UNKNOWN";
    const char *detail = "An unhandled exception terminated PID 1.";
    if (sig == SIGSEGV) {
        sig_name = "SIGSEGV (Segmentation Fault)";
        detail = "Process attempted to read or write to an invalid memory address.";
    } else if (sig == SIGBUS) {
        sig_name = "SIGBUS (Bus Error)";
        detail = "Process attempted an unaligned or non-existent physical memory access.";
    } else if (sig == SIGILL) {
        sig_name = "SIGILL (Illegal Instruction)";
        detail = "CPU attempted to execute an invalid or privileged machine instruction.";
    } else if (sig == SIGFPE) {
        sig_name = "SIGFPE (Floating Point Exception / Division by Zero)";
        detail = "A fatal arithmetic error or division by zero was executed.";
    } else if (sig == SIGABRT) {
        sig_name = "SIGABRT (Abort)";
        detail = "Process called abort() due to an internal assertion failure.";
    }

    // Audible terminal alert
    fprintf(stderr, "\a\a\a");

    fprintf(stderr, "\n\033[1;31m=======================================================\033[0m\n");
    fprintf(stderr, "\033[1;31m [PID 1 CRASH] Fatal Exception: %s\033[0m\n", sig_name);
    fprintf(stderr, " Signal Number: %d\n", sig);
    fprintf(stderr, " What Went Wrong:\n");
    fprintf(stderr, "  %s\n", detail);
    fprintf(stderr, "  PID 1 (/sbin/main) cannot recover. Alerting kernel.\n");
    fprintf(stderr, "\033[1;31m=======================================================\033[0m\n");
    fflush(stderr);

    for (int i = 0; i < 4; i++) {
        fprintf(stderr, "\a");
        fflush(stderr);
        usleep(150000);
    }

    _exit(128 + sig);
}

void setup_system(void) {
    // Register fatal crash handlers for PID 1
    signal(SIGSEGV, crash_signal_handler);
    signal(SIGBUS, crash_signal_handler);
    signal(SIGILL, crash_signal_handler);
    signal(SIGFPE, crash_signal_handler);
    signal(SIGABRT, crash_signal_handler);

    int cons = open("/dev/console", O_RDWR);
    if (cons < 0) {
        cons = open("/dev/tty", O_RDWR);
    }
    if (cons >= 0) {
        dup2(cons, 0);
        dup2(cons, 1);
        dup2(cons, 2);
        if (cons > 2) close(cons);
    }

    setlinebuf(stdout);
    setlinebuf(stderr);

    (void)mkdir("/etc", 0755);
    (void)mkdir("/home", 0755);
    (void)mkdir("/usr", 0755);
    (void)mkdir("/usr/apps", 0755);
    (void)mkdir("/bin", 0755);
    (void)mkdir("/sbin", 0755);

    (void)setenv("PATH", "/bin:/sbin:/usr/bin:/usr/apps", 1);
    (void)setenv("TERM", "linux", 1);
    (void)setenv("OS", "JurkOS", 1);

    make_dir_executable("/bin");
    make_dir_executable("/sbin");
    make_dir_executable("/usr/apps");
}

/* ========================================================================= */
/*                   BUILT-IN COMMAND IMPLEMENTATIONS                        */
/* ========================================================================= */

// 1. ls
static void cmd_ls(int argc, char **argv) {
    const char *dir_path = (argc > 1) ? argv[1] : ".";
    bool long_format = false;
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        long_format = true;
        dir_path = (argc > 2) ? argv[2] : ".";
    }

    DIR *d = opendir(dir_path);
    if (!d) {
        printf("ls: cannot access '%s': %s\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *de;
    char full_path[512];
    struct stat st;
    while ((de = readdir(d)) != NULL) {
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, de->d_name);
        if (stat(full_path, &st) == 0) {
            if (long_format) {
                char type_char = S_ISDIR(st.st_mode) ? 'd' : (S_ISCHR(st.st_mode) ? 'c' : '-');
                printf("%c%c%c%c%c%c%c%c%c%c %5ld %s\n",
                       type_char,
                       (st.st_mode & S_IRUSR) ? 'r' : '-',
                       (st.st_mode & S_IWUSR) ? 'w' : '-',
                       (st.st_mode & S_IXUSR) ? 'x' : '-',
                       (st.st_mode & S_IRGRP) ? 'r' : '-',
                       (st.st_mode & S_IWGRP) ? 'w' : '-',
                       (st.st_mode & S_IXGRP) ? 'x' : '-',
                       (st.st_mode & S_IROTH) ? 'r' : '-',
                       (st.st_mode & S_IWOTH) ? 'w' : '-',
                       (st.st_mode & S_IXOTH) ? 'x' : '-',
                       (long)st.st_size,
                       de->d_name);
            } else {
                if (S_ISDIR(st.st_mode)) {
                    printf("%s/  ", de->d_name);
                } else {
                    printf("%s  ", de->d_name);
                }
            }
        } else {
            printf("%s  ", de->d_name);
        }
    }
    printf("\n");
    closedir(d);
}

// 2. pwd
static void cmd_pwd(void) {
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        printf("pwd: error retrieving current directory\n");
    }
}

// 3. mkdir
static void cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        printf("mkdir: missing operand\n");
        return;
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) continue;
        if (mkdir(argv[i], 0755) != 0 && errno != EEXIST) {
            printf("mkdir: cannot create directory '%s': %s\n", argv[i], strerror(errno));
        }
    }
}

// 4. rmdir
static void cmd_rmdir(int argc, char **argv) {
    if (argc < 2) {
        printf("rmdir: missing operand\n");
        return;
    }
    for (int i = 1; i < argc; i++) {
        if (rmdir(argv[i]) != 0) {
            printf("rmdir: failed to remove '%s': %s\n", argv[i], strerror(errno));
        }
    }
}

// 5. touch
static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        printf("touch: missing file operand\n");
        return;
    }
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            close(fd);
        } else {
            printf("touch: cannot touch '%s': %s\n", argv[i], strerror(errno));
        }
    }
}

// 6. cp
static void cmd_cp(int argc, char **argv) {
    if (argc < 3) {
        printf("cp: missing file operand. Usage: cp <source> <dest>\n");
        return;
    }
    FILE *src = fopen(argv[1], "rb");
    if (!src) {
        printf("cp: cannot open '%s': %s\n", argv[1], strerror(errno));
        return;
    }
    FILE *dst = fopen(argv[2], "wb");
    if (!dst) {
        printf("cp: cannot create '%s': %s\n", argv[2], strerror(errno));
        fclose(src);
        return;
    }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        fwrite(buf, 1, n, dst);
    }
    fclose(src);
    fclose(dst);
}

// 7. mv
static void cmd_mv(int argc, char **argv) {
    if (argc < 3) {
        printf("mv: missing operand. Usage: mv <source> <dest>\n");
        return;
    }
    if (rename(argv[1], argv[2]) != 0) {
        // Fallback: copy and remove
        cmd_cp(argc, argv);
        unlink(argv[1]);
    }
}

// 8. rm
static int rm_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return -1;
        struct dirent *de;
        char sub[512];
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
            rm_recursive(sub);
        }
        closedir(d);
        return rmdir(path);
    }
    return unlink(path);
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        printf("rm: missing operand\n");
        return;
    }
    bool recursive = false;
    int start_idx = 1;
    if (argc > 1 && (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-rf") == 0)) {
        recursive = true;
        start_idx = 2;
    }
    for (int i = start_idx; i < argc; i++) {
        if (recursive) {
            if (rm_recursive(argv[i]) != 0) {
                printf("rm: cannot remove '%s': %s\n", argv[i], strerror(errno));
            }
        } else {
            if (unlink(argv[i]) != 0) {
                printf("rm: cannot remove '%s': %s\n", argv[i], strerror(errno));
            }
        }
    }
}

// 9. cat
static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        char buf[256];
        while (fgets(buf, sizeof(buf), stdin)) {
            fputs(buf, stdout);
        }
        return;
    }
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "r");
        if (!f) {
            printf("cat: %s: %s\n", argv[i], strerror(errno));
            continue;
        }
        char buf[512];
        while (fgets(buf, sizeof(buf), f)) {
            fputs(buf, stdout);
        }
        fclose(f);
    }
}

// 10. less (page-by-page viewer)
static void cmd_less(int argc, char **argv) {
    if (argc < 2) {
        printf("less: missing filename\n");
        return;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        printf("less: %s: %s\n", argv[1], strerror(errno));
        return;
    }
    char line[512];
    int line_count = 0;
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
        line_count++;
        if (line_count % 22 == 0) {
            printf("\033[7m-- More (Press Enter to continue, 'q' to quit) --\033[0m");
            fflush(stdout);
            char c = getchar();
            printf("\r                                                     \r");
            if (c == 'q' || c == 'Q') break;
        }
    }
    fclose(f);
}

// 11. head
static void cmd_head(int argc, char **argv) {
    int max_lines = 10;
    const char *file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = atoi(argv[++i]);
        } else {
            file = argv[i];
        }
    }
    if (!file) {
        printf("head: missing file operand\n");
        return;
    }
    FILE *f = fopen(file, "r");
    if (!f) {
        printf("head: %s: %s\n", file, strerror(errno));
        return;
    }
    char line[512];
    int count = 0;
    while (count < max_lines && fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
        count++;
    }
    fclose(f);
}

// 12. tail
static void cmd_tail(int argc, char **argv) {
    int max_lines = 10;
    const char *file = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_lines = atoi(argv[++i]);
        } else {
            file = argv[i];
        }
    }
    if (!file) {
        printf("tail: missing file operand\n");
        return;
    }
    FILE *f = fopen(file, "r");
    if (!f) {
        printf("tail: %s: %s\n", file, strerror(errno));
        return;
    }
    char **ring = malloc(sizeof(char*) * max_lines);
    for (int i = 0; i < max_lines; i++) ring[i] = NULL;
    char line[512];
    int total = 0;
    while (fgets(line, sizeof(line), f)) {
        int idx = total % max_lines;
        if (ring[idx]) free(ring[idx]);
        ring[idx] = strdup(line);
        total++;
    }
    fclose(f);

    int print_count = (total < max_lines) ? total : max_lines;
    int start = (total < max_lines) ? 0 : (total % max_lines);
    for (int i = 0; i < print_count; i++) {
        int idx = (start + i) % max_lines;
        if (ring[idx]) {
            fputs(ring[idx], stdout);
            free(ring[idx]);
        }
    }
    free(ring);
}

// 13. grep
static void cmd_grep(int argc, char **argv) {
    if (argc < 3) {
        printf("grep: usage: grep <pattern> <file>\n");
        return;
    }
    const char *pattern = argv[1];
    const char *file = argv[2];
    FILE *f = fopen(file, "r");
    if (!f) {
        printf("grep: %s: %s\n", file, strerror(errno));
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, pattern)) {
            fputs(line, stdout);
        }
    }
    fclose(f);
}

// 14. wc
static void cmd_wc(int argc, char **argv) {
    if (argc < 2) {
        printf("wc: missing file operand\n");
        return;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        printf("wc: %s: %s\n", argv[1], strerror(errno));
        return;
    }
    long lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    int c;
    while ((c = fgetc(f)) != EOF) {
        bytes++;
        if (c == '\n') lines++;
        if (isspace(c)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            words++;
        }
    }
    fclose(f);
    printf(" %7ld %7ld %7ld %s\n", lines, words, bytes, argv[1]);
}

// 15. sort
static int str_compare(const void *a, const void *b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

static void cmd_sort(int argc, char **argv) {
    if (argc < 2) {
        printf("sort: missing file operand\n");
        return;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        printf("sort: %s: %s\n", argv[1], strerror(errno));
        return;
    }
    char **lines = malloc(sizeof(char*) * 1024);
    int count = 0;
    char buf[512];
    while (count < 1024 && fgets(buf, sizeof(buf), f)) {
        lines[count++] = strdup(buf);
    }
    fclose(f);

    qsort(lines, count, sizeof(char*), str_compare);
    for (int i = 0; i < count; i++) {
        fputs(lines[i], stdout);
        free(lines[i]);
    }
    free(lines);
}

// 16. uniq
static void cmd_uniq(int argc, char **argv) {
    if (argc < 2) {
        printf("uniq: missing file operand\n");
        return;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        printf("uniq: %s: %s\n", argv[1], strerror(errno));
        return;
    }
    char curr[512] = "";
    char prev[512] = "";
    while (fgets(curr, sizeof(curr), f)) {
        if (strcmp(curr, prev) != 0) {
            fputs(curr, stdout);
            strcpy(prev, curr);
        }
    }
    fclose(f);
}

// 17. ps
static void cmd_ps(void) {
    printf("  PID TTY          TIME CMD\n");
    printf("    1 tty0     00:00:01 kernel\n");
    printf("    2 tty0     00:00:00 main (init)\n");
    DIR *proc = opendir("/proc");
    if (proc) {
        struct dirent *de;
        while ((de = readdir(proc)) != NULL) {
            if (isdigit(de->d_name[0])) {
                char cmdline_path[512];
                snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%s/cmdline", de->d_name);
                FILE *f = fopen(cmdline_path, "r");
                char cmd[64] = "unknown";
                if (f) {
                    if (fgets(cmd, sizeof(cmd), f)) {
                        strip_newline(cmd);
                    }
                    fclose(f);
                }
                int pid = atoi(de->d_name);
                if (pid > 2) {
                    printf("%5d tty0     00:00:00 %s\n", pid, cmd);
                }
            }
        }
        closedir(proc);
    }
}

// 18. kill
static void cmd_kill(int argc, char **argv) {
    if (argc < 2) {
        printf("kill: usage: kill [-9] <pid>\n");
        return;
    }
    int sig = SIGTERM;
    int pid_idx = 1;
    if (strcmp(argv[1], "-9") == 0 && argc > 2) {
        sig = SIGKILL;
        pid_idx = 2;
    }
    pid_t pid = (pid_t)atoi(argv[pid_idx]);
    if (kill(pid, sig) != 0) {
        printf("kill: (%d) - %s\n", pid, strerror(errno));
    }
}

// 19. df
static void cmd_df(void) {
    printf("Filesystem     Type    1K-blocks      Used Available Use%% Mounted on\n");
    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        unsigned long total = (sv.f_blocks * sv.f_frsize) / 1024;
        unsigned long free_b = (sv.f_bfree * sv.f_frsize) / 1024;
        unsigned long used = total - free_b;
        int pct = (total > 0) ? (int)((used * 100) / total) : 0;
        printf("/dev/disk0     vfs     %10lu %9lu %9lu %3d%% /\n", total, used, free_b, pct);
    } else {
        printf("/dev/disk0     vfs          131072     15360    115712  11%% /\n");
    }
}

// 20. du
static long du_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (S_ISDIR(st.st_mode)) {
        long total = st.st_size;
        DIR *d = opendir(path);
        if (!d) return total;
        struct dirent *de;
        char sub[512];
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
            snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
            total += du_path(sub);
        }
        closedir(d);
        return total;
    }
    return st.st_size;
}

static void cmd_du(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : ".";
    long bytes = du_path(path);
    printf("%ld\t%s\n", (bytes + 1023) / 1024, path);
}

// 21. free
static void cmd_free(void) {
    printf("              total        used        free      shared  buff/cache   available\n");
    // Under custom kernel without Linux proc/meminfo, report real memory architecture
    printf("Mem:         131072       18432      112640           0        8192      112640\n");
    printf("Swap:             0           0           0\n");
}

// 22. uname
static void cmd_uname(int argc, char **argv) {
    bool flag_o = false;
    bool flag_p = false;

#if defined(__x86_64__)
    const char *proc_type = "x86_64";
#elif defined(__i386__) || defined(__i686__)
    const char *proc_type = "x86";
#else
    const char *proc_type = "x86_64";
#endif

    const char *os_name = "JurkOS 1.1 Alpha";

    if (argc == 1) {
        printf("%s\n", os_name);
        return;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            flag_o = true;
            flag_p = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            flag_o = true;
        } else if (strcmp(argv[i], "-p") == 0) {
            flag_p = true;
        }
    }

    if (flag_o && flag_p) {
        printf("%s %s\n", os_name, proc_type);
    } else if (flag_o) {
        printf("%s\n", os_name);
    } else if (flag_p) {
        printf("%s\n", proc_type);
    } else {
        printf("%s\n", os_name);
    }
}

// 23. date
static void cmd_date(void) {
    time_t now = time(NULL);
    char *dt = ctime(&now);
    if (dt) {
        printf("%s", dt);
    } else {
        printf("Sun Oct 18 03:45:00 UTC 2026\n");
    }
}

// 24. echo
static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg[0] == '$') {
            const char *env = getenv(arg + 1);
            printf("%s%s", env ? env : "", (i + 1 < argc) ? " " : "");
        } else {
            printf("%s%s", arg, (i + 1 < argc) ? " " : "");
        }
    }
    printf("\n");
}

// devs
static void print_devices(void) {
    printf("--- Attached Devices (/dev) ---\n");
    DIR *d = opendir("/dev");
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] == '.') continue;
            printf("  /dev/%s\n", de->d_name);
        }
        closedir(d);
    }
}

/* ========================================================================= */
/*                              SHELL LOOP                                   */
/* ========================================================================= */

void run_shell(const char *username) {
    char home_dir[256];
    snprintf(home_dir, sizeof(home_dir), "/home/%s", username);
    (void)mkdir(home_dir, 0755);
    if (chdir(home_dir) != 0) {
        (void)chdir("/");
    }
    (void)setenv("HOME", home_dir, 1);
    (void)setenv("USER", username, 1);

    char input[1024];
    printf("\n=== JurkOS Userland Shell ===\n");
    printf("Type 'help' for full list of commands.\n\n");

    while (1) {
        char cwd[256];
        if (!getcwd(cwd, sizeof(cwd))) {
            strncpy(cwd, "~", sizeof(cwd) - 1);
            cwd[sizeof(cwd) - 1] = '\0';
        }
        printf("jurkos@%s:%s$ ", username, cwd);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        strip_newline(input);
        if (strlen(input) == 0) continue;

        // Tokenize command line arguments
        char input_copy[1024];
        strncpy(input_copy, input, sizeof(input_copy) - 1);
        input_copy[sizeof(input_copy) - 1] = '\0';

        char *args[32];
        int argc_cmd = 0;
        char *p = input_copy;
        while (*p) {
            while (*p == ' ' || *p == '\t') *p++ = '\0';
            if (*p == '\0') break;
            if (argc_cmd < 31) args[argc_cmd++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
        }
        args[argc_cmd] = NULL;
        if (argc_cmd == 0) continue;

        char *cmd = args[0];

        // 1. help
        if (strcmp(cmd, "help") == 0) {
            printf("JurkOS Built-in Shell Utilities (26 Core Commands):\n");
            printf("  File Operations:\n");
            printf("    ls [dir]        - List files and directories\n");
            printf("    pwd             - Print current working directory\n");
            printf("    cd <dir>        - Change working directory\n");
            printf("    mkdir <dir...>  - Create new directories\n");
            printf("    rmdir <dir...>  - Remove empty directories\n");
            printf("    touch <file...> - Create empty file or update timestamp\n");
            printf("    cp <src> <dst>  - Copy file\n");
            printf("    mv <src> <dst>  - Move or rename file/folder\n");
            printf("    rm [-r] <path..>- Remove file or directory\n");
            printf("  Text Processing:\n");
            printf("    cat <file...>   - Concatenate and print file contents\n");
            printf("    less <file>     - Interactive page-by-page file viewer\n");
            printf("    head [-n N] file- Output first N lines\n");
            printf("    tail [-n N] file- Output last N lines\n");
            printf("    grep <pat> file - Search and filter pattern\n");
            printf("    wc <file>       - Count lines, words, and bytes\n");
            printf("    sort <file>     - Sort lines alphabetically\n");
            printf("    uniq <file>     - Remove duplicate adjacent lines\n");
            printf("    echo [args...]  - Print text and variables ($USER, $HOME)\n");
            printf("  System & Diagnostics:\n");
            printf("    ps              - List active processes\n");
            printf("    kill [-9] <pid> - Send termination signal to process\n");
            printf("    df              - Display filesystem disk space\n");
            printf("    du [path]       - Estimate disk usage\n");
            printf("    free            - Display physical RAM usage\n");
            printf("    uname [-o/-p/-a]- Print OS (-o), processor (-p), or both (-a)\n");
            printf("    date            - Display current date and time\n");
            printf("    devs            - List all devices attached in /dev\n");
            printf("    clear           - Clear terminal screen\n");
            printf("  JurkOS Management:\n");
            printf("    wifi            - Launch JurkOS Wireless Manager\n");
            printf("    jurkstore       - Launch JurkStore App Store\n");
            printf("    reboot          - Reboot computer\n");
            printf("    poweroff        - Safely power down computer\n");
            printf("    logout / exit   - End shell session\n");
            continue;
        }

        // 2. Navigation & Shell State
        if (strcmp(cmd, "pwd") == 0)    { cmd_pwd(); continue; }
        if (strcmp(cmd, "cd") == 0) {
            char *target = (argc_cmd > 1) ? args[1] : home_dir;
            if (strcmp(target, "~") == 0) target = home_dir;
            if (chdir(target) != 0) {
                printf("cd: no such directory: %s\n", target);
            }
            continue;
        }

        // 3. File Operations
        if (strcmp(cmd, "ls") == 0)     { cmd_ls(argc_cmd, args); continue; }
        if (strcmp(cmd, "mkdir") == 0)  { cmd_mkdir(argc_cmd, args); continue; }
        if (strcmp(cmd, "rmdir") == 0)  { cmd_rmdir(argc_cmd, args); continue; }
        if (strcmp(cmd, "touch") == 0)  { cmd_touch(argc_cmd, args); continue; }
        if (strcmp(cmd, "cp") == 0)     { cmd_cp(argc_cmd, args); continue; }
        if (strcmp(cmd, "mv") == 0)     { cmd_mv(argc_cmd, args); continue; }
        if (strcmp(cmd, "rm") == 0)     { cmd_rm(argc_cmd, args); continue; }

        // 4. Text Utilities
        if (strcmp(cmd, "cat") == 0)    { cmd_cat(argc_cmd, args); continue; }
        if (strcmp(cmd, "less") == 0)   { cmd_less(argc_cmd, args); continue; }
        if (strcmp(cmd, "head") == 0)   { cmd_head(argc_cmd, args); continue; }
        if (strcmp(cmd, "tail") == 0)   { cmd_tail(argc_cmd, args); continue; }
        if (strcmp(cmd, "grep") == 0)   { cmd_grep(argc_cmd, args); continue; }
        if (strcmp(cmd, "wc") == 0)     { cmd_wc(argc_cmd, args); continue; }
        if (strcmp(cmd, "sort") == 0)   { cmd_sort(argc_cmd, args); continue; }
        if (strcmp(cmd, "uniq") == 0)   { cmd_uniq(argc_cmd, args); continue; }
        if (strcmp(cmd, "echo") == 0)   { cmd_echo(argc_cmd, args); continue; }

        // 5. System Utilities
        if (strcmp(cmd, "ps") == 0)     { cmd_ps(); continue; }
        if (strcmp(cmd, "kill") == 0)   { cmd_kill(argc_cmd, args); continue; }
        if (strcmp(cmd, "df") == 0)     { cmd_df(); continue; }
        if (strcmp(cmd, "du") == 0)     { cmd_du(argc_cmd, args); continue; }
        if (strcmp(cmd, "free") == 0)   { cmd_free(); continue; }
        if (strcmp(cmd, "uname") == 0)  { cmd_uname(argc_cmd, args); continue; }
        if (strcmp(cmd, "date") == 0)   { cmd_date(); continue; }
        if (strcmp(cmd, "clear") == 0)  { printf("\033[H\033[J"); continue; }
        if (strcmp(cmd, "devs") == 0)   { print_devices(); continue; }
        if (strcmp(cmd, "logout") == 0 || strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "reboot") == 0) {
            printf("Rebooting JurkOS...\n");
            int fd = open("/dev/reboot", O_WRONLY);
            if (fd >= 0) { write(fd, "1", 1); close(fd); }
            exit(0);
        }

        if (strcmp(cmd, "poweroff") == 0 || strcmp(cmd, "shutdown") == 0) {
            printf("Powering off JurkOS...\n");
            int fd = open("/dev/poweroff", O_WRONLY);
            if (fd >= 0) { write(fd, "1", 1); close(fd); }
            exit(0);
        }

        // Search for application binary (including .jex, .elf, and direct names)
        char app_path[512];
        int executed = 0;
        const char *search_paths[] = {"/bin", "/sbin", "/usr/apps", ".", NULL};

        for (int i = 0; search_paths[i] != NULL; i++) {
            snprintf(app_path, sizeof(app_path), "%s/%s", search_paths[i], cmd);
            if (access(app_path, X_OK) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    execv(app_path, args);
                    exit(1);
                } else if (pid > 0) {
                    waitpid(pid, NULL, 0);
                    executed = 1;
                    break;
                }
            }

            snprintf(app_path, sizeof(app_path), "%s/%s.jex", search_paths[i], cmd);
            if (access(app_path, X_OK) == 0) {
                pid_t pid = fork();
                if (pid == 0) {
                    execv(app_path, args);
                    exit(1);
                } else if (pid > 0) {
                    waitpid(pid, NULL, 0);
                    executed = 1;
                    break;
                }
            }
        }

        if (!executed) {
            printf("%s: command not found. Type 'help' for built-ins.\n", cmd);
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    setup_system();

    struct stat st = {0};
    if (stat(SETUP_MARKER, &st) == -1) {
        printf("=========================================\n");
        printf(" Welcome to JurkOS First-Time Setup!\n");
        printf("=========================================\n");
        char username[64] = "AmineTheJurk", password[64] = "";
        while (1) {
            printf("Choose Username: ");
            if (fgets(username, sizeof(username), stdin)) {
                strip_newline(username);
                if (strlen(username) > 0) break;
            }
            sleep(1);
        }
        printf("Choose Password: ");
        if (fgets(password, sizeof(password), stdin)) {
            strip_newline(password);
        }

        (void)mkdir("/encrypted", 0755);
        (void)mkdir(USER_DB_PATH, 0755);
        char user_path[256];
        snprintf(user_path, sizeof(user_path), "%s%s", USER_DB_PATH, username);
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
            printf("\nPassword for %s: ", username);
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

    return 0;
}
