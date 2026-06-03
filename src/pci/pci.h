#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_ADDR(bus, dev, func, reg) \
    ((u32)(1u << 31) | \
    (((u32)(bus) & 0xFFu) << 16) | \
    (((u32)(dev) & 0x1Fu) << 11) | \
    (((u32)(func) & 0x07u) << 8) | \
    ((u32)(reg) & 0xFCu))

#define PCI_REG_VENDOR_ID     0x00
#define PCI_REG_DEVICE_ID     0x02
#define PCI_REG_COMMAND       0x04
#define PCI_REG_STATUS        0x06
#define PCI_REG_REVISION      0x08
#define PCI_REG_PROG_IF       0x09
#define PCI_REG_SUBCLASS      0x0A
#define PCI_REG_CLASS         0x0B
#define PCI_REG_HEADER_TYPE   0x0E
#define PCI_REG_BAR0          0x10
#define PCI_REG_BAR5          0x24
#define PCI_REG_SUBSYS_VENDOR 0x2C
#define PCI_REG_SUBSYS_ID     0x2E
#define PCI_REG_IRQ_LINE      0x3C
#define PCI_REG_IRQ_PIN       0x3D

#define PCI_CMD_IO_SPACE   (1u << 0)
#define PCI_CMD_MEM_SPACE  (1u << 1)
#define PCI_CMD_BUS_MASTER (1u << 2)

#define PCI_HEADER_NORMAL     0x00
#define PCI_HEADER_MULTI_FUNC 0x80

#define PCI_VENDOR_NONE 0xFFFF

#define PCI_MAX_DEV      32
#define PCI_MAX_FUNC     8
#define PCI_MAX_BARS     6
#define PCI_MAX_DEVICES  64

typedef enum {
    PCI_BAR_NONE = 0,
    PCI_BAR_IO,
    PCI_BAR_MEM32,
    PCI_BAR_MEM64,
} pci_bar_type_t;

typedef struct {
    pci_bar_type_t type;
    u64 base;
    u64 size;
    bool prefetch;
} pci_bar_t;

typedef struct {
    u8 bus;
    u8 device;
    u8 function;

    u16 vendor_id;
    u16 device_id;
    u16 subsys_vendor;
    u16 subsys_id;

    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 revision;

    pci_bar_t bars[PCI_MAX_BARS];

    u8 irq_line;
    u8 irq_pin;

    bool valid;
    bool multifunction;
} pci_device_t;

typedef struct {
    pci_device_t devices[PCI_MAX_DEVICES];
    size_t count;
} pci_bus_t;

void pci_enumerate(pci_bus_t *out);

pci_device_t *pci_find_device(
    pci_bus_t *bus,
    u16 vendor_id,
    u16 device_id
);

pci_device_t *pci_find_class(
    pci_bus_t *bus,
    u8 class_code,
    u8 subclass
);

u32 pci_cfg_read32(u8 bus, u8 dev, u8 func, u8 reg);
u16 pci_cfg_read16(u8 bus, u8 dev, u8 func, u8 reg);
u8  pci_cfg_read8 (u8 bus, u8 dev, u8 func, u8 reg);

void pci_cfg_write32(u8 bus, u8 dev, u8 func, u8 reg, u32 val);
void pci_cfg_write16(u8 bus, u8 dev, u8 func, u8 reg, u16 val);
void pci_cfg_write8 (u8 bus, u8 dev, u8 func, u8 reg, u8 val);

void pci_enable_io(const pci_device_t *d);
void pci_enable_mmio(const pci_device_t *d);
void pci_enable_busmaster(const pci_device_t *d);

#endif
