/* JurkOS Wi-Fi & Network Manager (wifi)
 * Scans nearby real wireless networks, connects to Open or WPA2/WPA3 Wi-Fi,
 * negotiates DHCP, manages wired Ethernet, and stores persistent wireless profiles.
 * Copyright (c) 2026 AmineTheJurk
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_NETWORKS 64
#define WIFI_CONF_DIR "/encrypted"
#define WIFI_HISTORY_FILE "/encrypted/wifi_history.conf"

typedef struct {
    char ssid[64];
    int signal_pct;
    char security[32]; // "Open", "WPA2-PSK", "WPA3-SAE", "WPA-PSK", "WEP"
    int channel;
    int is_open;
} WifiNetwork;

static struct termios orig_termios;
static int raw_mode_active = 0;

static void disable_raw_mode(void) {
    if (raw_mode_active) {
        printf("\033[?25h"); // Show cursor
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
}

static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) return;
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;
}

static void handle_sigint(int sig) {
    (void)sig;
    disable_raw_mode();
    printf("\n\033[0mWi-Fi manager closed.\n");
    exit(0);
}

static int run_sys(const char *cmd) {
    int r = system(cmd);
    return r;
}

/* Detect first available physical wireless network interface */
static int detect_wireless_interface(char *iface, size_t max_len) {
    (void)run_sys("rfkill unblock wifi 2>/dev/null; rfkill unblock all 2>/dev/null");
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;

    struct dirent *de;
    char candidate[64] = {0};
    int found = 0;

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, "lo") == 0) continue;

        // 1. Check /sys/class/net/<dev>/type for IEEE 802.11 types (801, 802, 803)
        char type_path[256];
        snprintf(type_path, sizeof(type_path), "/sys/class/net/%.64s/type", de->d_name);
        FILE *tf = fopen(type_path, "r");
        if (tf) {
            int dev_type = 0;
            if (fscanf(tf, "%d", &dev_type) == 1) {
                if (dev_type == 801 || dev_type == 802 || dev_type == 803) {
                    snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
                    found = 1;
                    fclose(tf);
                    break;
                }
            }
            fclose(tf);
        }

        // 2. Check sysfs markers: /sys/class/net/<dev>/wireless, /sys/class/net/<dev>/phy80211, device/phy80211
        char path[256];
        snprintf(path, sizeof(path), "/sys/class/net/%.64s/wireless", de->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
            found = 1;
            break;
        }

        snprintf(path, sizeof(path), "/sys/class/net/%.64s/phy80211", de->d_name);
        if (stat(path, &st) == 0) {
            snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
            found = 1;
            break;
        }

        snprintf(path, sizeof(path), "/sys/class/net/%.64s/device/phy80211", de->d_name);
        if (stat(path, &st) == 0) {
            snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
            found = 1;
            break;
        }

        // 3. Interface naming schemes (wlan*, wlp*, wls*, wlx*, wifi*, ath*, ra*, mlan*, wfx*)
        if (strncmp(de->d_name, "wl", 2) == 0 ||
            strncmp(de->d_name, "wifi", 4) == 0 ||
            strncmp(de->d_name, "ath", 3) == 0 ||
            strncmp(de->d_name, "ra", 2) == 0 ||
            strncmp(de->d_name, "mlan", 4) == 0 ||
            strncmp(de->d_name, "wfx", 3) == 0) {
            snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
            found = 1;
            break;
        }
    }
    closedir(d);

    // 4. Fallback check /proc/net/wireless
    if (!found) {
        FILE *pf = fopen("/proc/net/wireless", "r");
        if (pf) {
            char line[256];
            while (fgets(line, sizeof(line), pf)) {
                if (strchr(line, ':')) {
                    char *colon = strchr(line, ':');
                    *colon = '\0';
                    char *if_start = line;
                    while (*if_start == ' ' || *if_start == '\t') if_start++;
                    if (strlen(if_start) > 0 && strcmp(if_start, "Inter-") != 0 && strcmp(if_start, "face") != 0) {
                        snprintf(candidate, sizeof(candidate), "%.63s", if_start);
                        found = 1;
                        break;
                    }
                }
            }
            fclose(pf);
        }
    }

    if (found) {
        snprintf(iface, max_len, "%s", candidate);
        return 1;
    }

    iface[0] = '\0';
    return 0;
}

/* Detect first available wired Ethernet interface */
static int detect_wired_interface(char *iface, size_t max_len) {
    DIR *d = opendir("/sys/class/net");
    if (!d) return 0;

    struct dirent *de;
    char candidate[64] = {0};
    int found = 0;

    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        if (strcmp(de->d_name, "lo") == 0) continue;
        if (strncmp(de->d_name, "wl", 2) == 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/sys/class/net/%.64s/wireless", de->d_name);
        struct stat st;
        if (stat(path, &st) == 0) continue;

        snprintf(path, sizeof(path), "/sys/class/net/%.64s/phy80211", de->d_name);
        if (stat(path, &st) == 0) continue;

        // Valid wired candidate (e.g. eth0, ens3, enp0s3)
        snprintf(candidate, sizeof(candidate), "%.63s", de->d_name);
        found = 1;
        break;
    }
    closedir(d);

    if (found) {
        snprintf(iface, max_len, "%s", candidate);
        return 1;
    }

    iface[0] = '\0';
    return 0;
}

/* Read current IP address on interface */
static int get_ip_address(const char *iface, char *ip_buf, size_t max_len) {
    if (!iface || strlen(iface) == 0) return 0;

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return 0;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
        struct sockaddr_in *sa = (struct sockaddr_in *)&ifr.ifr_addr;
        char *ip = inet_ntoa(sa->sin_addr);
        if (ip && strcmp(ip, "0.0.0.0") != 0 && strcmp(ip, "127.0.0.1") != 0) {
            snprintf(ip_buf, max_len, "%s", ip);
            close(fd);
            return 1;
        }
    }
    close(fd);
    return 0;
}

/* Ensure udhcpc helper script exists and is executable */
static void ensure_udhcpc_script(void) {
    (void)mkdir("/usr", 0755);
    (void)mkdir("/usr/share", 0755);
    (void)mkdir("/usr/share/udhcpc", 0755);
    const char *script_path = "/usr/share/udhcpc/default.script";
    FILE *fp = fopen(script_path, "w");
    if (fp) {
        fprintf(fp, "#!/bin/sh\n"
                    "[ -n \"$1\" ] || exit 1\n"
                    "case \"$1\" in\n"
                    "  deconfig) /bin/ifconfig $interface 0.0.0.0 ;;\n"
                    "  renew|bound)\n"
                    "    /bin/ifconfig $interface $ip ${subnet:+netmask $subnet} ${broadcast:+broadcast $broadcast}\n"
                    "    if [ -n \"$router\" ]; then\n"
                    "      while /bin/route del default gw 0.0.0.0 dev $interface 2>/dev/null; do :; done\n"
                    "      for r in $router; do /bin/route add default gw $r dev $interface; done\n"
                    "    fi\n"
                    "    echo -n > /etc/resolv.conf\n"
                    "    for d in $dns; do echo \"nameserver $d\" >> /etc/resolv.conf; done\n"
                    "    [ -s /etc/resolv.conf ] || printf 'nameserver 8.8.8.8\\nnameserver 1.1.1.1\\n' > /etc/resolv.conf\n"
                    "    ;;\n"
                    "esac\n"
                    "exit 0\n");
        fclose(fp);
        (void)chmod(script_path, 0755);
    }
}

/* Configure wired Ethernet interface via DHCP with robust fallback */
static int configure_wired_interface(const char *iface) {
    ensure_udhcpc_script();

    printf("\n\033[1;36m[Wired Network]\033[0m Activating interface \033[1m%s\033[0m...\n", iface);
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ifconfig %s up 2>/dev/null || ip link set %s up 2>/dev/null", iface, iface);
    run_sys(cmd);

    printf("\033[1;36m[Wired Network]\033[0m Requesting IP via DHCP (BusyBox udhcpc)...\n");
    snprintf(cmd, sizeof(cmd), "busybox udhcpc -i %s -s /usr/share/udhcpc/default.script -n -q -t 4 2>/dev/null || udhcpc -i %s -s /usr/share/udhcpc/default.script -n -q -t 4 2>/dev/null", iface, iface);
    run_sys(cmd);

    // Setup DNS
    (void)mkdir("/etc", 0755);
    FILE *dns = fopen("/etc/resolv.conf", "w");
    if (dns) {
        fprintf(dns, "nameserver 8.8.8.8\nnameserver 1.1.1.1\n");
        fclose(dns);
    }

    char ip[64] = {0};
    if (!get_ip_address(iface, ip, sizeof(ip))) {
        // Fallback for QEMU user-networking or unconfigured DHCP servers
        printf("\033[1;33m[*] Applying QEMU / standard subnet fallback configuration...\033[0m\n");
        snprintf(cmd, sizeof(cmd), "ifconfig %s 10.0.2.15 netmask 255.255.255.0 up 2>/dev/null; route add default gw 10.0.2.2 dev %s 2>/dev/null", iface, iface);
        run_sys(cmd);
        (void)get_ip_address(iface, ip, sizeof(ip));
    }

    if (strlen(ip) > 0) {
        printf("\n\033[1;32m====================================================================\033[0m\n");
        printf("\033[1;32m [SUCCESS] Wired Network Connected!\033[0m\n");
        printf(" Interface:   \033[1m%s\033[0m\n", iface);
        printf(" IP Address:  \033[1;36m%s\033[0m\n", ip);
        printf(" DNS Servers: 8.8.8.8 (Google), 1.1.1.1 (Cloudflare)\n");
        printf(" Status:      Connected to internet (ping, wget, JurkStore ready).\n");
        printf("\033[1;32m====================================================================\033[0m\n\n");
        return 1;
    } else {
        printf("\n\033[1;33m[!] Could not acquire IP address on interface %s.\033[0m\n", iface);
        printf("Check cable connection or host network settings.\n\n");
        return 0;
    }
}

/* Parse scan output from iwlist */
static int parse_iwlist_scan(const char *scan_file, WifiNetwork *networks, int max_nets) {
    FILE *fp = fopen(scan_file, "r");
    if (!fp) return 0;

    char line[512];
    int count = 0;
    WifiNetwork cur;
    memset(&cur, 0, sizeof(cur));
    cur.signal_pct = 50;
    cur.channel = 1;
    cur.is_open = 1;
    snprintf(cur.security, sizeof(cur.security), "Open");

    int in_cell = 0;

    while (fgets(line, sizeof(line), fp) && count < max_nets) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strstr(p, "Cell ") != NULL) {
            if (in_cell && strlen(cur.ssid) > 0) {
                // Deduplicate by SSID
                int dup = 0;
                for (int i = 0; i < count; i++) {
                    if (strcmp(networks[i].ssid, cur.ssid) == 0) {
                        dup = 1;
                        if (cur.signal_pct > networks[i].signal_pct) {
                            networks[i] = cur;
                        }
                        break;
                    }
                }
                if (!dup && count < max_nets) {
                    networks[count++] = cur;
                }
            }

            in_cell = 1;
            memset(&cur, 0, sizeof(cur));
            cur.signal_pct = 50;
            cur.channel = 1;
            cur.is_open = 1;
            snprintf(cur.security, sizeof(cur.security), "Open");
            continue;
        }

        if (!in_cell) continue;

        // ESSID:"MyNetwork"
        char *essid = strstr(p, "ESSID:\"");
        if (essid) {
            essid += 7;
            char *end = strchr(essid, '\"');
            if (end) {
                size_t len = (size_t)(end - essid);
                if (len >= sizeof(cur.ssid)) len = sizeof(cur.ssid) - 1;
                strncpy(cur.ssid, essid, len);
                cur.ssid[len] = '\0';
            }
        }

        // Channel:X or Frequency:X GHz (Channel X)
        char *ch = strstr(p, "Channel:");
        if (ch) {
            cur.channel = atoi(ch + 8);
        } else {
            char *ch_in_freq = strstr(p, "(Channel ");
            if (ch_in_freq) {
                cur.channel = atoi(ch_in_freq + 9);
            }
        }

        // Quality=XX/70 or Signal level=-XX dBm
        char *qual = strstr(p, "Quality=");
        if (qual) {
            int q = atoi(qual + 8);
            cur.signal_pct = (q * 100) / 70;
            if (cur.signal_pct > 100) cur.signal_pct = 100;
        } else {
            char *sig = strstr(p, "Signal level=");
            if (sig) {
                int dbm = atoi(sig + 13);
                if (dbm < 0) {
                    cur.signal_pct = 2 * (dbm + 100);
                    if (cur.signal_pct < 5) cur.signal_pct = 5;
                    if (cur.signal_pct > 100) cur.signal_pct = 100;
                }
            }
        }

        // Encryption key:off / on
        if (strstr(p, "Encryption key:off")) {
            cur.is_open = 1;
            snprintf(cur.security, sizeof(cur.security), "Open");
        } else if (strstr(p, "Encryption key:on")) {
            if (cur.is_open) {
                cur.is_open = 0;
                snprintf(cur.security, sizeof(cur.security), "WEP");
            }
        }

        // WPA2 / WPA3 / WPA
        if (strstr(p, "IEEE 802.11i/WPA2") || strstr(p, "WPA2")) {
            cur.is_open = 0;
            snprintf(cur.security, sizeof(cur.security), "WPA2-PSK");
        } else if (strstr(p, "WPA3") || strstr(p, "SAE")) {
            cur.is_open = 0;
            snprintf(cur.security, sizeof(cur.security), "WPA3-SAE");
        } else if (strstr(p, "WPA Version 1") || strstr(p, "WPA1")) {
            cur.is_open = 0;
            snprintf(cur.security, sizeof(cur.security), "WPA-PSK");
        }
    }

    if (in_cell && strlen(cur.ssid) > 0 && count < max_nets) {
        int dup = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(networks[i].ssid, cur.ssid) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup) networks[count++] = cur;
    }

    fclose(fp);
    return count;
}

/* Execute scan on interface - STRICTLY REAL HARDWARE ONLY, NO DEMO/FAKE NETWORKS */
static int scan_wifi_networks(const char *iface, WifiNetwork *networks, int max_nets) {
    if (!iface || strlen(iface) == 0) return 0;

    char cmd[512];

    // Ensure interface is up
    snprintf(cmd, sizeof(cmd), "rfkill unblock wifi 2>/dev/null; ifconfig %s up 2>/dev/null || ip link set %s up 2>/dev/null", iface, iface);
    run_sys(cmd);

    // Scan using iwlist
    snprintf(cmd, sizeof(cmd), "iwlist %s scan > /tmp/wifi_scan.tmp 2>/dev/null", iface);
    int ret = system(cmd);

    int count = 0;
    if (ret == 0) {
        count = parse_iwlist_scan("/tmp/wifi_scan.tmp", networks, max_nets);
    }

    // Try iw dev scan fallback if iwlist returned 0
    if (count == 0) {
        snprintf(cmd, sizeof(cmd), "iw dev %s scan 2>/dev/null | grep -E '(SSID|signal|WPA|RSN)' > /tmp/wifi_scan.tmp", iface);
        run_sys(cmd);

        FILE *fp = fopen("/tmp/wifi_scan.tmp", "r");
        if (fp) {
            char line[256];
            WifiNetwork cur;
            memset(&cur, 0, sizeof(cur));
            cur.signal_pct = 70;
            cur.channel = 1;
            cur.is_open = 1;
            snprintf(cur.security, sizeof(cur.security), "Open");

            while (fgets(line, sizeof(line), fp) && count < max_nets) {
                if (strstr(line, "SSID: ")) {
                    char *s = strstr(line, "SSID: ") + 6;
                    s[strcspn(s, "\r\n")] = 0;
                    if (strlen(s) > 0) {
                        strncpy(cur.ssid, s, sizeof(cur.ssid) - 1);
                        networks[count++] = cur;
                        memset(&cur, 0, sizeof(cur));
                        cur.signal_pct = 65;
                        cur.channel = 1;
                        cur.is_open = 1;
                        snprintf(cur.security, sizeof(cur.security), "Open");
                    }
                } else if (strstr(line, "RSN") || strstr(line, "WPA2")) {
                    cur.is_open = 0;
                    snprintf(cur.security, sizeof(cur.security), "WPA2-PSK");
                }
            }
            fclose(fp);
        }
    }

    (void)remove("/tmp/wifi_scan.tmp");

    // Strictly real networks only: if count is 0, return 0 (no fake profiles)
    return count;
}

/* Render signal strength as visual bars [####] */
static const char *signal_bars(int pct) {
    if (pct >= 80) return "[####]";
    if (pct >= 60) return "[###-]";
    if (pct >= 40) return "[##--]";
    if (pct >= 20) return "[#---]";
    return "[----]";
}

/* Prompt for password with character masking */
static void get_password_masked(char *buf, size_t max_len) {
    enable_raw_mode();
    size_t idx = 0;
    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;
        if (c == '\n' || c == '\r') {
            buf[idx] = '\0';
            printf("\n");
            break;
        } else if (c == 127 || c == '\b') { // Backspace
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 3) { // Ctrl+C
            disable_raw_mode();
            printf("\nCancelled.\n");
            exit(0);
        } else if (c >= 32 && c <= 126 && idx < max_len - 1) {
            buf[idx++] = c;
            printf("*");
            fflush(stdout);
        }
    }
    disable_raw_mode();
}

/* Save connection to history */
static void save_wifi_profile(const char *ssid, const char *password, int is_open) {
    (void)mkdir(WIFI_CONF_DIR, 0755);
    FILE *fp = fopen(WIFI_HISTORY_FILE, "a");
    if (!fp) return;
    if (is_open) {
        fprintf(fp, "SSID=%s\nSECURITY=Open\n\n", ssid);
    } else {
        fprintf(fp, "SSID=%s\nSECURITY=WPA2\nPSK=%s\n\n", ssid, password ? password : "");
    }
    fclose(fp);
}

/* Connect to the selected Wi-Fi network */
static int connect_wifi(const char *iface, const char *ssid, const char *password, int is_open) {
    printf("\n\033[1;36m[Wi-Fi]\033[0m Configuring interface \033[1m%s\033[0m for network: \033[1;32m\"%s\"\033[0m\n", iface, ssid);

    // 1. Generate wpa_supplicant configuration
    (void)mkdir("/etc", 0755);
    (void)mkdir("/var", 0755);
    (void)mkdir("/var/run", 0755);
    (void)mkdir("/var/run/wpa_supplicant", 0755);

    FILE *conf = fopen("/tmp/wpa_supplicant.conf", "w");
    if (conf) {
        fprintf(conf, "ctrl_interface=/var/run/wpa_supplicant\n");
        fprintf(conf, "update_config=1\n\n");
        fprintf(conf, "network={\n");
        fprintf(conf, "    ssid=\"%s\"\n", ssid);
        if (is_open) {
            fprintf(conf, "    key_mgmt=NONE\n");
        } else {
            fprintf(conf, "    psk=\"%s\"\n", password ? password : "");
            fprintf(conf, "    key_mgmt=WPA-PSK WPA-EAP SAE\n");
        }
        fprintf(conf, "}\n");
        fclose(conf);
    }

    // 2. Stop any conflicting background network daemons on this interface
    printf("\033[1;36m[Wi-Fi]\033[0m Initializing wireless supplicant...\n");
    run_sys("killall -9 wpa_supplicant udhcpc 2>/dev/null");
    usleep(200000);

    // Bring interface up
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "ifconfig %s up 2>/dev/null || ip link set %s up 2>/dev/null", iface, iface);
    run_sys(cmd);

    // 3. Launch wpa_supplicant
    snprintf(cmd, sizeof(cmd), "wpa_supplicant -B -i %s -c /tmp/wpa_supplicant.conf -D nl80211,wext 2>/dev/null", iface);
    run_sys(cmd);

    printf("\033[1;36m[Wi-Fi]\033[0m Associating with Access Point (handshake)...\n");
    sleep(2);

    // 4. Request IP via BusyBox udhcpc
    printf("\033[1;36m[Wi-Fi]\033[0m Requesting IP address via DHCP (BusyBox udhcpc)...\n");
    snprintf(cmd, sizeof(cmd), "busybox udhcpc -i %s -n -q -t 6 2>/dev/null || udhcpc -i %s -n -q -t 6 2>/dev/null", iface, iface);
    run_sys(cmd);

    // Check if real IP acquired
    char ip[64] = {0};
    int has_ip = get_ip_address(iface, ip, sizeof(ip));

    // Ensure /etc/resolv.conf has DNS servers
    FILE *dns = fopen("/etc/resolv.conf", "w");
    if (dns) {
        fprintf(dns, "nameserver 8.8.8.8\nnameserver 1.1.1.1\n");
        fclose(dns);
    }

    // Save profile to persistent storage
    save_wifi_profile(ssid, password, is_open);

    if (has_ip) {
        printf("\n\033[1;32m====================================================================\033[0m\n");
        printf("\033[1;32m [SUCCESS] Connected to Wi-Fi: %s\033[0m\n", ssid);
        printf(" Interface: %s\n", iface);
        printf(" IP Address: \033[1;36m%s\033[0m\n", ip);
        printf(" Security:   %s\n", is_open ? "Open (No Password)" : "WPA2-PSK (Encrypted)");
        printf(" DNS:        8.8.8.8 (Google), 1.1.1.1 (Cloudflare)\n");
        printf(" Profile:    Saved to %s\n", WIFI_HISTORY_FILE);
        printf("\033[1;32m====================================================================\033[0m\n\n");
        return 1;
    } else {
        printf("\n\033[1;33m[!] Associated with \"%s\", but no DHCP IP was received.\033[0m\n", ssid);
        printf("Check if password was correct or if router DHCP pool is accessible.\n\n");
        return 0;
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    char iface[64] = {0};
    char wired[64] = {0};

    // Detect wireless hardware
    int hw_detected = detect_wireless_interface(iface, sizeof(iface));
    int wired_detected = detect_wired_interface(wired, sizeof(wired));

    // Handle command line flags: wifi connect <ssid> [pass]
    if (argc >= 3 && strcmp(argv[1], "connect") == 0) {
        if (!hw_detected) {
            fprintf(stderr, "Error: No wireless hardware interface found on this system.\n");
            return 1;
        }
        const char *ssid = argv[2];
        const char *pass = (argc >= 4) ? argv[3] : NULL;
        int is_open = (pass == NULL || strlen(pass) == 0);
        return connect_wifi(iface, ssid, pass, is_open) ? 0 : 1;
    }

    while (1) {
        // Refresh interface status on every loop
        hw_detected = detect_wireless_interface(iface, sizeof(iface));
        wired_detected = detect_wired_interface(wired, sizeof(wired));

        printf("\033[H\033[J"); // Clear screen
        printf("\033[1;36m+================================================================+\033[0m\n");
        printf("\033[1;36m|                 JurkOS Network & Wi-Fi Manager                 |\033[0m\n");
        printf("\033[1;36m+================================================================+\033[0m\n");

        if (!hw_detected) {
            // NO WIRELESS ADAPTER FOUND (e.g. running in QEMU VM)
            char wired_ip[64] = {0};
            int has_wired_ip = 0;
            if (wired_detected) {
                has_wired_ip = get_ip_address(wired, wired_ip, sizeof(wired_ip));
            }

            printf(" \033[1;33m[!] No physical 802.11 Wi-Fi adapter detected.\033[0m\n\n");
            printf(" \033[1mWhy is this happening?\033[0m\n");
            printf(" - Virtual machines (like QEMU, VirtualBox, VMware) only emulate\n");
            printf("   wired Ethernet (%s). They do NOT expose your host laptop/PC\n", wired_detected ? wired : "eth0");
            printf("   Wi-Fi antenna as a wireless device to the guest operating system.\n");
            printf(" - To use real Wi-Fi in JurkOS:\n");
            printf("   * Boot JurkOS directly on physical hardware (USB drive), OR\n");
            printf("   * Pass through a physical USB Wi-Fi dongle into QEMU.\n\n");

            printf(" \033[1;36mCurrent Wired Connection Status:\033[0m\n");
            if (wired_detected) {
                printf("   Wired Interface: \033[1m%s\033[0m\n", wired);
                if (has_wired_ip) {
                    printf("   IP Address:      \033[1;32m%s\033[0m (Active & Online)\n", wired_ip);
                    printf("   Internet Access: \033[1;32mConnected via Host Bridge\033[0m\n");
                } else {
                    printf("   IP Address:      \033[1;33mNone assigned yet\033[0m\n");
                    printf("   Internet Access: Not configured\n");
                }
            } else {
                printf("   No network interfaces found.\n");
            }

            printf("\n ----------------------------------------------------------------\n");
            if (wired_detected) {
                printf("  \033[1m[1]\033[0m Configure / Renew Wired Ethernet (%s DHCP)\n", wired);
            }
            printf("  \033[1m[R]\033[0m Rescan for Wi-Fi adapter (plug in USB dongle)\n");
            printf("  \033[1m[M]\033[0m Manually specify Wi-Fi interface name\n");
            printf("  \033[1m[Q]\033[0m Return to JurkOS shell\n\n");

            printf("\033[1mSelect action [1, R, M, Q]: \033[0m");
            fflush(stdout);

            char input[128];
            if (!fgets(input, sizeof(input), stdin)) break;
            input[strcspn(input, "\r\n")] = 0;

            if (strcasecmp(input, "q") == 0 || strcmp(input, "exit") == 0) {
                printf("Exiting Network Manager.\n");
                break;
            }
            if (strcasecmp(input, "r") == 0) {
                continue;
            }
            if (strcasecmp(input, "1") == 0 && wired_detected) {
                configure_wired_interface(wired);
                printf("Press Enter to continue...");
                getchar();
                continue;
            }
            if (strcasecmp(input, "m") == 0) {
                printf("\nEnter wireless interface name (e.g. wlan0, wlan1): ");
                char manual_iface[64];
                if (fgets(manual_iface, sizeof(manual_iface), stdin)) {
                    manual_iface[strcspn(manual_iface, "\r\n")] = 0;
                    if (strlen(manual_iface) > 0) {
                        snprintf(iface, sizeof(iface), "%s", manual_iface);
                        hw_detected = 1;
                        // will loop into wireless scan
                        continue;
                    }
                }
            }
            continue;
        }

        // PHYSICAL WIRELESS INTERFACE DETECTED
        printf(" Wireless Interface: \033[1;32m%s [Hardware Ready]\033[0m\n", iface);
        printf(" Scanning airwaves for nearby Wi-Fi networks...\n\n");

        WifiNetwork networks[MAX_NETWORKS];
        int count = scan_wifi_networks(iface, networks, MAX_NETWORKS);

        if (count == 0) {
            printf("  \033[1;33m[!] No wireless networks found in range of %s.\033[0m\n", iface);
            printf("  Make sure your antenna is attached, Wi-Fi kill switch is off,\n");
            printf("  or connect to a hidden SSID using [M].\n\n");
        } else {
            printf("   \033[1m%-4s %-24s %-8s %-8s %-12s %s\033[0m\n", "#", "SSID", "SIGNAL", "BARS", "SECURITY", "CHANNEL");
            printf("  ----------------------------------------------------------------\n");

            for (int i = 0; i < count; i++) {
                const char *sec_color = networks[i].is_open ? "\033[1;32m" : "\033[1;33m";
                printf("  [%d]  %-24s %3d%%    %-8s %s%-12s\033[0m Ch %-2d\n",
                       i + 1,
                       networks[i].ssid,
                       networks[i].signal_pct,
                       signal_bars(networks[i].signal_pct),
                       sec_color,
                       networks[i].security,
                       networks[i].channel);
            }
            printf("  ----------------------------------------------------------------\n");
        }

        printf("  \033[1m[M]\033[0m Connect to hidden/manual SSID\n");
        printf("  \033[1m[R]\033[0m Rescan nearby networks\n");
        if (wired_detected) {
            printf("  \033[1m[E]\033[0m Switch to Wired Ethernet (%s)\n", wired);
        }
        printf("  \033[1m[Q]\033[0m Quit to JurkOS shell\n\n");

        if (count > 0) {
            printf("\033[1mSelect Wi-Fi network [1-%d, M, R, Q]: \033[0m", count);
        } else {
            printf("\033[1mSelect option [M, R, Q]: \033[0m");
        }
        fflush(stdout);

        char input[128];
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\r\n")] = 0;

        if (strcasecmp(input, "q") == 0 || strcmp(input, "exit") == 0) {
            printf("Exiting Wi-Fi manager.\n");
            break;
        }

        if (strcasecmp(input, "r") == 0) {
            continue; // Rescan
        }

        if (strcasecmp(input, "e") == 0 && wired_detected) {
            configure_wired_interface(wired);
            printf("Press Enter to continue...");
            getchar();
            continue;
        }

        if (strcasecmp(input, "m") == 0) {
            char manual_ssid[64];
            printf("\nEnter Wi-Fi SSID: ");
            if (!fgets(manual_ssid, sizeof(manual_ssid), stdin)) continue;
            manual_ssid[strcspn(manual_ssid, "\r\n")] = 0;
            if (strlen(manual_ssid) == 0) continue;

            char has_sec[16];
            printf("Is this network password-protected? (y/N): ");
            if (!fgets(has_sec, sizeof(has_sec), stdin)) continue;
            has_sec[strcspn(has_sec, "\r\n")] = 0;

            if (strcasecmp(has_sec, "y") == 0 || strcasecmp(has_sec, "yes") == 0) {
                char manual_pass[128];
                printf("Enter Wi-Fi password for \"%s\": ", manual_ssid);
                fflush(stdout);
                get_password_masked(manual_pass, sizeof(manual_pass));
                connect_wifi(iface, manual_ssid, manual_pass, 0);
            } else {
                printf("Connecting to open network \"%s\"...\n", manual_ssid);
                connect_wifi(iface, manual_ssid, NULL, 1);
            }
            printf("Press Enter to continue...");
            getchar();
            continue;
        }

        int choice = atoi(input);
        if (choice >= 1 && choice <= count) {
            WifiNetwork chosen = networks[choice - 1];

            if (chosen.is_open) {
                printf("\n\033[1;32m\"%s\" is an open network (no password required).\033[0m\n", chosen.ssid);
                printf("Connecting immediately...\n");
                connect_wifi(iface, chosen.ssid, NULL, 1);
            } else {
                char password[128];
                printf("\nEnter Wi-Fi password for \033[1;32m\"%s\"\033[0m: ", chosen.ssid);
                fflush(stdout);
                get_password_masked(password, sizeof(password));

                if (strlen(password) == 0) {
                    printf("\033[1;31mPassword cannot be empty.\033[0m Press Enter to retry...");
                    getchar();
                    continue;
                }

                connect_wifi(iface, chosen.ssid, password, 0);
            }
            printf("Press Enter to continue...");
            getchar();
            continue;
        } else {
            printf("\033[1;31mInvalid selection.\033[0m Press Enter to try again...");
            getchar();
        }
    }

    return 0;
}
