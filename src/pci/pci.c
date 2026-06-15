#ifndef PCI_DRIVER_H
#define PCI_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "pci.h"
#include "../kernel.h"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static inline void _outl(u16 port, u32 val) {
    __asm__ volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 _inl(u16 port) {
    u32 val;
    __asm__ volatile ("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void _outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t _inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

inline u32 pci_cfg_read32(u8 bus, u8 dev, u8 func, u8 reg) {
    _outl(PCI_CONFIG_ADDRESS, PCI_ADDR(bus, dev, func, reg));
    return _inl(PCI_CONFIG_DATA);
}

inline u16 pci_cfg_read16(u8 bus, u8 dev, u8 func, u8 reg) {
    u32 dword = pci_cfg_read32(bus, dev, func, reg & ~0x3u);
    return (u16)(dword >> ((reg & 2u) * 8u));
}

inline u8 pci_cfg_read8(u8 bus, u8 dev, u8 func, u8 reg) {
    u32 dword = pci_cfg_read32(bus, dev, func, reg & ~0x3u);
    return (u8)(dword >> ((reg & 3u) * 8u));
}

inline void pci_cfg_write32(u8 bus, u8 dev, u8 func, u8 reg, u32 val) {
    _outl(PCI_CONFIG_ADDRESS, PCI_ADDR(bus, dev, func, reg));
    _outl(PCI_CONFIG_DATA, val);
}

inline void pci_cfg_write16(u8 bus, u8 dev, u8 func, u8 reg, u16 val) {
    u32 dword = pci_cfg_read32(bus, dev, func, reg & ~0x3u);
    u32 shift = (reg & 2u) * 8u;
    dword = (dword & ~(0xFFFFu << shift)) | ((u32)val << shift);
    pci_cfg_write32(bus, dev, func, reg & ~0x3u, dword);
}

inline void pci_cfg_write8(u8 bus, u8 dev, u8 func, u8 reg, u8 val) {
    u32 dword = pci_cfg_read32(bus, dev, func, reg & ~0x3u);
    u32 shift = (reg & 3u) * 8u;
    dword = (dword & ~(0xFFu << shift)) | ((u32)val << shift);
    pci_cfg_write32(bus, dev, func, reg & ~0x3u, dword);
}

static inline bool pci_probe_bar(u8 bus, u8 dev, u8 func,
    u8 idx, pci_bar_t *out) {
    u8  reg  = (u8)(PCI_REG_BAR0 + idx * 4u);
    u32 orig = pci_cfg_read32(bus, dev, func, reg);
    if (orig == 0xFFFFFFFFu) {
        out->type = PCI_BAR_NONE;
        out->base = 0; out->size = 0; out->prefetch = false;
        return false;
    }

    pci_cfg_write32(bus, dev, func, reg, 0xFFFFFFFFu);
    u32 mask = pci_cfg_read32(bus, dev, func, reg);
    pci_cfg_write32(bus, dev, func, reg, orig);

    if (mask == 0 || mask == 0xFFFFFFFFu) {
        out->type = PCI_BAR_NONE;
        out->base = 0; out->size = 0; out->prefetch = false;
        return false;
    }

    if (orig & 0x1u) {
        out->type = PCI_BAR_IO;
        out->base = (u64)(orig & 0xFFFCu);
        u32 size_mask = mask & 0xFFFCu;
        out->size = size_mask ? (u64)(~size_mask + 1u) : 0;
        out->prefetch = false;
    } else {
        u8 width = (u8)((orig >> 1u) & 0x3u);
        out->prefetch = ((orig >> 3u) & 0x1u) != 0;
        if (width == 0x2u) {
            u8  reg_hi  = (u8)(reg + 4u);
            u32 orig_hi = pci_cfg_read32(bus, dev, func, reg_hi);
            pci_cfg_write32(bus, dev, func, reg_hi, 0xFFFFFFFFu);
            u32 mask_hi = pci_cfg_read32(bus, dev, func, reg_hi);
            pci_cfg_write32(bus, dev, func, reg_hi, orig_hi);
            u64 full_mask = ((u64)mask_hi << 32u) | (u64)(mask & 0xFFFFFFF0u);
            out->type = PCI_BAR_MEM64;
            out->base = ((u64)orig_hi << 32u) | (u64)(orig & 0xFFFFFFF0u);
            out->size = full_mask ? ~full_mask + 1u : 0;
        } else {
            u32 addr_mask = mask & 0xFFFFFFF0u;
            out->type = PCI_BAR_MEM32;
            out->base = (u64)(orig & 0xFFFFFFF0u);
            out->size = addr_mask ? (u64)(~addr_mask + 1u) : 0;
        }
    }
    return true;
}

static inline bool pci_scan_device(u8 bus, u8 dev,
                                   u8 func, pci_device_t *out) {
    u16 vendor = pci_cfg_read16(bus, dev, func, PCI_REG_VENDOR_ID);
    if (vendor == PCI_VENDOR_NONE)
        return false;

    out->bus      = bus;
    out->device   = dev;
    out->function = func;
    out->valid    = true;

    out->vendor_id     = vendor;
    out->device_id     = pci_cfg_read16(bus, dev, func, PCI_REG_DEVICE_ID);
    out->class_code    = pci_cfg_read8 (bus, dev, func, PCI_REG_CLASS);
    out->subclass      = pci_cfg_read8 (bus, dev, func, PCI_REG_SUBCLASS);
    out->prog_if       = pci_cfg_read8 (bus, dev, func, PCI_REG_PROG_IF);
    out->revision      = pci_cfg_read8 (bus, dev, func, PCI_REG_REVISION);
    out->subsys_vendor = pci_cfg_read16(bus, dev, func, PCI_REG_SUBSYS_VENDOR);
    out->subsys_id     = pci_cfg_read16(bus, dev, func, PCI_REG_SUBSYS_ID);
    out->irq_line      = pci_cfg_read8 (bus, dev, func, PCI_REG_IRQ_LINE);
    out->irq_pin       = pci_cfg_read8 (bus, dev, func, PCI_REG_IRQ_PIN);

    u8 header = pci_cfg_read8(bus, dev, func, PCI_REG_HEADER_TYPE);
    out->multifunction = (header & PCI_HEADER_MULTI_FUNC) != 0;

    /* Clear BARs */
    for (u8 i = 0; i < PCI_MAX_BARS; i++) {
        out->bars[i].type = PCI_BAR_NONE;
        out->bars[i].base = 0;
        out->bars[i].size = 0;
        out->bars[i].prefetch = false;
    }
    if ((header & 0x7Fu) == PCI_HEADER_NORMAL) {
        for (u8 i = 0; i < PCI_MAX_BARS; ) {
            if (!pci_probe_bar(bus, dev, func, i, &out->bars[i])) {
                i++; continue;
            }
            if (out->bars[i].type == PCI_BAR_MEM64 && i + 1 < PCI_MAX_BARS) {
                out->bars[i + 1].type = PCI_BAR_NONE; // mark high dword as consumed
                i += 2; // skip both lo and hi
            } else {
                i++;
            }
        }
    }

    return true;
}

static inline void zero_mem(void *ptr, u32 size) {
    u8 *p = (u8 *)ptr;
    for (u32 i = 0; i < size; i++) p[i] = 0;
}

static inline void copy_mem(void *dst, const void *src, u32 size) {
    u8 *d = (u8 *)dst;
    const u8 *s = (const u8 *)src;
    for (u32 i = 0; i < size; i++) d[i] = s[i];
}

static pci_device_t _pci_tmp;

static void dbg(const char *msg) {
    console_writeln(msg);
}

static void dbg_ok(void) {
    console_writeln(" OK");
}

static void dbg_fail(void) {
    console_writeln(" FAIL");
}

inline void pci_enumerate(pci_bus_t *out) {
    if (!out) return;
    out->count = 0;
    zero_mem(out->devices, sizeof(out->devices));

    for (u8 bus = 0; bus < 4; bus++) {
        for (u8 dev = 0; dev < PCI_MAX_DEV; dev++) {
            u16 vendor = pci_cfg_read16(bus, dev, 0, PCI_REG_VENDOR_ID);
            if (vendor == 0xFFFFu || vendor == 0x0000u) continue;

            u8 header  = pci_cfg_read8(bus, dev, 0, PCI_REG_HEADER_TYPE);
            u8 max_func = (header & PCI_HEADER_MULTI_FUNC) ? PCI_MAX_FUNC : 1;

            for (u8 func = 0; func < max_func; func++) {
                vendor = pci_cfg_read16(bus, dev, func, PCI_REG_VENDOR_ID);
                if (vendor == 0xFFFFu || vendor == 0x0000u) continue;
                if (out->count >= PCI_MAX_DEVICES) return;

                pci_device_t *d = &out->devices[out->count];
                zero_mem(d, sizeof(*d));
                if (!pci_scan_device(bus, dev, func, d)) continue;
                if (d->vendor_id == 0xFFFFu || d->vendor_id == 0) continue;

                out->count++;

                console_write("PCI ");
                if (d->vendor_id == 0x8086)      console_write("Intel");
                else if (d->vendor_id == 0x10DE) console_write("NVIDIA");
                else if (d->vendor_id == 0x1002) console_write("AMD");
                else if (d->vendor_id == 0x1234) console_write("BOCHS");
                else                             console_write("????");

                console_write(" class:");
                if      (d->class_code == 0x01) console_write("storage");
                else if (d->class_code == 0x02) console_write("net");
                else if (d->class_code == 0x03) console_write("display");
                else if (d->class_code == 0x06) console_write("bridge");
                else                            console_write("?");
                console_write("\n");
            }
        }
    }
    console_write("PCI done\n");
}

inline pci_device_t *pci_find_device(pci_bus_t *bus,
                                            u16 vendor_id, u16 device_id) {
    for (size_t i = 0; i < bus->count; i++) {
        pci_device_t *d = &bus->devices[i];
        if ((vendor_id == 0 || d->vendor_id == vendor_id) &&
            (device_id == 0 || d->device_id == device_id))
            return d;
    }
    return NULL;
}

inline pci_device_t *pci_find_class(pci_bus_t *bus,
                                           u8 class_code, u8 subclass) {
    for (size_t i = 0; i < bus->count; i++) {
        pci_device_t *d = &bus->devices[i];
        if (d->class_code == class_code && d->subclass == subclass)
            return d;
    }
    return NULL;
}

inline void pci_enable_io(const pci_device_t *d) {
    u16 cmd = pci_cfg_read16(d->bus, d->device, d->function, PCI_REG_COMMAND);
    pci_cfg_write16(d->bus, d->device, d->function,
                    PCI_REG_COMMAND, cmd | PCI_CMD_IO_SPACE);
}

inline void pci_enable_mmio(const pci_device_t *d) {
    u16 cmd = pci_cfg_read16(d->bus, d->device, d->function, PCI_REG_COMMAND);
    pci_cfg_write16(d->bus, d->device, d->function,
                    PCI_REG_COMMAND, cmd | PCI_CMD_MEM_SPACE);
}

inline void pci_enable_busmaster(const pci_device_t *d) {
    u16 cmd = pci_cfg_read16(d->bus, d->device, d->function, PCI_REG_COMMAND);
    pci_cfg_write16(d->bus, d->device, d->function,
                    PCI_REG_COMMAND, cmd | PCI_CMD_BUS_MASTER);
}

#endif /* PCI_DRIVER_H */
