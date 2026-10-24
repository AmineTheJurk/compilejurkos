#include "../include/e1000.h"
#include "../include/io.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/serial.h"

#define REG_CTRL        0x0000
#define REG_STATUS      0x0008
#define REG_EEPROM      0x0014
#define REG_RCTRL       0x0100
#define REG_RXDESCLO    0x2800
#define REG_RXDESCHI    0x2804
#define REG_RXDESCLEN   0x2808
#define REG_RXDESCHEAD  0x2810
#define REG_RXDESCTAIL  0x2818
#define REG_TCTRL       0x0400
#define REG_TXDESCLO    0x3800
#define REG_TXDESCHI    0x3804
#define REG_TXDESCLEN   0x3808
#define REG_TXDESCHEAD  0x3810
#define REG_TXDESCTAIL  0x3818

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 32

struct e1000_rx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint16_t checksum;
    volatile uint8_t  status;
    volatile uint8_t  errors;
    volatile uint16_t special;
} __attribute__((packed));

struct e1000_tx_desc {
    volatile uint64_t addr;
    volatile uint16_t length;
    volatile uint8_t  cso;
    volatile uint8_t  cmd;
    volatile uint8_t  status;
    volatile uint8_t  css;
    volatile uint16_t special;
} __attribute__((packed));

static uintptr_t mmio_base = 0;
static uint8_t mac_addr[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static struct e1000_rx_desc *rx_descs = NULL;
static struct e1000_tx_desc *tx_descs = NULL;
static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;

static inline void write_reg(uint16_t reg, uint32_t val) {
    *((volatile uint32_t*)(mmio_base + reg)) = val;
}

static inline uint32_t read_reg(uint16_t reg) {
    return *((volatile uint32_t*)(mmio_base + reg));
}

static uint16_t read_eeprom(uint8_t addr) {
    write_reg(REG_EEPROM, 1 | ((uint32_t)(addr) << 8));
    uint32_t val;
    while (!((val = read_reg(REG_EEPROM)) & (1 << 4)));
    return (uint16_t)((val >> 16) & 0xFFFF);
}

bool e1000_init(struct pci_device *pci_dev) {
    if (!pci_dev) return false;

    // Enable Bus Mastering & Memory Space
    uint32_t pci_cmd = pci_read_config(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04);
    pci_cmd |= 0x06;
    pci_write_config(pci_dev->bus, pci_dev->slot, pci_dev->func, 0x04, pci_cmd);

    mmio_base = pci_dev->bar[0] & ~0xFULL;

    // Read MAC address
    uint16_t m0 = read_eeprom(0);
    uint16_t m1 = read_eeprom(1);
    uint16_t m2 = read_eeprom(2);
    if (m0 != 0xFFFF && m1 != 0xFFFF && m2 != 0xFFFF) {
        mac_addr[0] = m0 & 0xFF;
        mac_addr[1] = m0 >> 8;
        mac_addr[2] = m1 & 0xFF;
        mac_addr[3] = m1 >> 8;
        mac_addr[4] = m2 & 0xFF;
        mac_addr[5] = m2 >> 8;
    }

    serial_printf("e1000: Initialized at MMIO 0x%x, MAC: %x:%x:%x:%x:%x:%x\n",
                  mmio_base, mac_addr[0], mac_addr[1], mac_addr[2],
                  mac_addr[3], mac_addr[4], mac_addr[5]);

    // Allocate descriptor rings
    rx_descs = (struct e1000_rx_desc*)pmm_alloc_page();
    tx_descs = (struct e1000_tx_desc*)pmm_alloc_page();
    memset(rx_descs, 0, PAGE_SIZE);
    memset(tx_descs, 0, PAGE_SIZE);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        rx_descs[i].addr = (uint64_t)(uintptr_t)pmm_alloc_page();
        rx_descs[i].status = 0;
    }

    write_reg(REG_RXDESCLO, (uint32_t)(uintptr_t)rx_descs);
    write_reg(REG_RXDESCHI, 0);
    write_reg(REG_RXDESCLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    write_reg(REG_RXDESCHEAD, 0);
    write_reg(REG_RXDESCTAIL, E1000_NUM_RX_DESC - 1);

    write_reg(REG_TXDESCLO, (uint32_t)(uintptr_t)tx_descs);
    write_reg(REG_TXDESCHI, 0);
    write_reg(REG_TXDESCLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    write_reg(REG_TXDESCHEAD, 0);
    write_reg(REG_TXDESCTAIL, 0);

    // Enable RX and TX
    write_reg(REG_RCTRL, (1 << 1) | (1 << 3) | (1 << 4) | (1 << 15));
    write_reg(REG_TCTRL, (1 << 1) | (1 << 3) | (0xF << 4) | (0x40 << 12));

    return true;
}

int e1000_send_packet(const uint8_t *packet, uint16_t length) {
    if (!tx_descs) return -1;

    void *tx_buf = (void*)(uintptr_t)tx_descs[tx_cur].addr;
    if (!tx_buf) {
        tx_buf = pmm_alloc_page();
        tx_descs[tx_cur].addr = (uint64_t)(uintptr_t)tx_buf;
    }

    memcpy(tx_buf, packet, length);
    tx_descs[tx_cur].length = length;
    tx_descs[tx_cur].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP | IFCS | RS
    tx_descs[tx_cur].status = 0;

    uint16_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    write_reg(REG_TXDESCTAIL, tx_cur);

    while (!(tx_descs[old_cur].status & 0xFF));
    return length;
}

int e1000_receive_packet(uint8_t *buffer, uint16_t max_length) {
    if (!rx_descs) return -1;
    if (!(rx_descs[rx_cur].status & 0x01)) return 0; // Not ready

    uint16_t len = rx_descs[rx_cur].length;
    if (len > max_length) len = max_length;

    memcpy(buffer, (const void*)(uintptr_t)rx_descs[rx_cur].addr, len);
    rx_descs[rx_cur].status = 0;

    uint16_t old_cur = rx_cur;
    rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
    write_reg(REG_RXDESCTAIL, old_cur);

    return len;
}

const uint8_t *e1000_get_mac(void) {
    return mac_addr;
}
