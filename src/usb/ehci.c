/*
 * [Cygnus] - [src/ehci.c]
 *
 * Copyright (C) [2025] [Szymon Grajner]
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the European Union Public Licence (EUPL) V.1.2 or - as
 * soon as they will be approved by the European Commission - subsequent
 * versions of the EUPL (the "Licence").
 *
 * You may not use this work except in compliance with the Licence.
 * You may obtain a copy of the Licence at:
 * https://joinup.ec.europa.eu/software/page/eupl/licence-eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 */
#include "../../inc/usb/ehci.h"
#include "../../inc/usb/usb_types.h"
#include "../../inc/usb/usb_core.h"

#define PCI_CLASS_SERIAL_BUS 0x0C
#define PCI_SUBCLASS_USB     0x03
#define PCI_PROGIF_EHCI      0x20
#define EHCI_PCI_BAR         0x10

typedef struct PACKED {
    u8 CAPLENGTH;
    u8 reserved;
    u16 HCIVERSION;
    u32 HCSParams;
    u32 HCCParams;
    u32 HCSPortRoute;
} ehci_cap_t;

typedef struct PACKED {
    u32 USBCMD;
    u32 USBSTS;
    u32 USBINTR;
    u32 FRINDEX;
    u32 CTRLDSSEGMENT;
    u32 PERIODICLISTBASE;
    u32 ASYNCLISTADDR;
    u32 rsvd[9];
    u32 CONFIGFLAG;
    u32 PORTSC[]; // zmienna długość
} ehci_op_t;

static bool pci_match_ehci(u32 b,u32 d,u32 f){
    u32 cc = pci_read32(b,d,f,0x08);
    u8 base=(cc>>24)&0xFF, sub=(cc>>16)&0xFF, prog=(cc>>8)&0xFF;
    return base==PCI_CLASS_SERIAL_BUS && sub==PCI_SUBCLASS_USB && prog==PCI_PROGIF_EHCI;
}
static u64 pci_get_bar(u32 b,u32 d,u32 f,u32 off){
    u32 lo=pci_read32(b,d,f,off);
    if (!lo) return 0;
    return (u64)(lo & ~0xFu);
}
static void* map_mmio(u64 phys){ return (void*)(uintptr_t)phys; }

static void ehci_bringup(u32 b,u32 d,u32 f){
    u64 bar = pci_get_bar(b,d,f,EHCI_PCI_BAR);
    if (!bar) return;
    volatile ehci_cap_t* cap = (volatile ehci_cap_t*)map_mmio(bar);
    volatile ehci_op_t*  op  = (volatile ehci_op_t*)((volatile u8*)cap + cap->CAPLENGTH);

    // Zatrzymaj + reset
    op->USBCMD &= ~1u;
    for (volatile int i=0;i<100000;i++);
    op->USBCMD |= (1u<<1); // HCReset
    for (volatile int i=0;i<100000;i++);

    // Ustaw CONFIGFLAG = 1 (przekieruj porty do EHCI)
    op->CONFIGFLAG = 1;

    // Zasil i zresetuj porty (best-effort)
    u32 nports = cap->HCSParams & 0xF;
    for (u32 p=0;p<nports;p++){
        volatile u32* ps = &op->PORTSC[p];
        *ps |= (1u<<12); // PP — zasilanie portu
        *ps |= (1u<<8);  // PR — reset portu
    }

    // Uruchom
    op->USBCMD |= 1u;

    // Log portów
    for (u32 p=0;p<nports;p++){
        volatile u32* ps = &op->PORTSC[p];
        u32 v=*ps;
        u32 conn=v&1, enabled=(v>>2)&1;
        klog("ehci","port %u: conn=%u enabled=%u\n", p+1, conn, enabled);
    }

    usb_hc_t* hc = (usb_hc_t*)kzalloc(sizeof(usb_hc_t));
    hc->kind = USB_HCI_EHCI;
    hc->impl = (void*)cap;
    usb_register_hc(hc);
    klog("ehci","controller @ %02x:%02x.%u online (skeleton).\n", b,d,f);
}

void ehci_probe_all(void){
    for (u32 b=0;b<256;b++)
    for (u32 d=0;d<32;d++)
    for (u32 f=0;f<8;f++){
        u32 vd=pci_read32(b,d,f,0x00);
        if (vd==0xFFFFFFFF) continue;
        if (pci_match_ehci(b,d,f)) ehci_bringup(b,d,f);
    }
}