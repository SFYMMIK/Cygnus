/*
 * [Cygnus] - [src/xhci.c]
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
#include "../../inc/usb/xhci.h"
#include "../../inc/usb/usb_types.h"
#include "../../inc/usb/usb_core.h"

// ========= Pomocnicze =========
static inline void write64(volatile u64* r, u64 v){ *r = v; __asm__ __volatile__("":::"memory"); }
static inline u64  read64 (volatile u64* r){ return *r; }

#define XHCI_RING_TRBS 256
#define XHCI_DB_HOST   0
#define XHCI_INTR0_OFF 0x20

// Bity PortSC
#define PORTSC_CCS      (1u<<0)
#define PORTSC_PED      (1u<<1)
#define PORTSC_OCA      (1u<<3)
#define PORTSC_PR       (1u<<4)
#define PORTSC_PP       (1u<<9)
#define PORTSC_PL_SHIFT 5
#define PORTSC_PL_MASK  (0xFu<<PORTSC_PL_SHIFT)

// Typy TRB komend / eventów
#define TRB_LINK            6
#define TRB_ENABLE_SLOT     9
#define TRB_ADDRESS_DEV     11

#define TRB_SETUP_STAGE     2
#define TRB_DATA_STAGE      3
#define TRB_STATUS_STAGE    4

#define TRB_CMD_COMPLETION  33
#define TRB_PORT_STATUS     34
#define TRB_TRANSFER_EVENT  32

// Flagi TRB (wycinek)
#define TRB_CYCLE_BIT       (1u<<0)
#define TRB_ENT             (1u<<1)   // Evaluate Next TRB
#define TRB_IOC             (1u<<5)   // Interrupt On Completion
#define TRB_IDT             (1u<<6)   // Immediate Data (dla Setup)
#define TRB_DIR_IN          (1u<<16)  // dla Data Stage IN

// ERST wpis
typedef struct PACKED {
    u64 ring_base;
    u32 ring_size;
    u32 rsvd;
} xhci_erst_entry_t;

// ======= PCI skan =======
#define PCI_CLASS_SERIAL_BUS 0x0C
#define PCI_SUBCLASS_USB     0x03
#define PCI_PROGIF_XHCI      0x30
#define XHCI_PCI_BAR         0x10

static bool pci_match_xhci(u32 bus,u32 dev,u32 fun) {
    u32 classcode = pci_read32(bus,dev,fun,0x08);
    u8 base=(classcode>>24)&0xFF, sub=(classcode>>16)&0xFF, prog=(classcode>>8)&0xFF;
    return base==PCI_CLASS_SERIAL_BUS && sub==PCI_SUBCLASS_USB && prog==PCI_PROGIF_XHCI;
}

static u64 pci_get_bar_mmio(u32 bus,u32 dev,u32 fun,u32 off) {
    u32 lo = pci_read32(bus,dev,fun,off);
    if (!lo) return 0;
    if ((lo & 0x04)) { // 64-bit BAR
        u32 hi = pci_read32(bus,dev,fun,off+4);
        return (((u64)hi)<<32) | (lo & ~0xFu);
    } else {
        return (u64)(lo & ~0xFu);
    }
}

static void* map_mmio(u64 phys, u32* len_out) {
    if (len_out) *len_out = 0x4000;
    return (void*)(uintptr_t)phys; // tożsamościowo – podmień wg swojego MM
}

// ======= Ringi =======
static void ring_init(xhci_ring_t* r, u32 count){
    r->trbs = NULL; r->phys=0; r->idx=0; r->cycle=1; r->count=count;
}

static void ring_alloc_dma(xhci_ring_t* r){
    r->trbs = (xhci_trb_t*)kalloc_dma(sizeof(xhci_trb_t)*r->count,&r->phys);
    r->idx = 0; r->cycle = 1;
    // Link TRB na końcu do początku, TC=1
    xhci_trb_t* last = &r->trbs[r->count-1];
    last->p0 = (u32)(r->phys & 0xFFFFFFFF);
    last->p1 = (u32)(r->phys >> 32);
    last->p2 = 0;
    last->p3 = (TRB_LINK<<10) | (1u<<1); // Toggle Cycle
}

static void ring_push(xhci_ring_t* r, u32 p0,u32 p1,u32 p2,u32 p3){
    xhci_trb_t* t = &r->trbs[r->idx];
    t->p0=p0; t->p1=p1; t->p2=p2;
    t->p3=(p3 & ~1u) | (r->cycle & 1u);
    r->idx++;
    if (r->idx == r->count-1){ r->idx=0; r->cycle^=1; }
}

static void cmd_ring_doorbell(xhci_t* x){ mmio32_write(&x->db->DB, 0, 0); }

// ======= Bring-up =======
static void xhci_reset(xhci_t* x) {
    if (!(mmio32_read(&x->op->USBSTS,0) & XHCI_USBSTS_HCH)) {
        u32 cmd = mmio32_read(&x->op->USBCMD,0);
        mmio32_write(&x->op->USBCMD,0, cmd & ~1u);
        for (int i=0;i<1000000;i++){
            if (mmio32_read(&x->op->USBSTS,0) & XHCI_USBSTS_HCH) break;
        }
    }
    mmio32_write(&x->op->USBCMD,0, XHCI_USBCMD_HCRST);
    for (int i=0;i<1000000;i++){
        if (!(mmio32_read(&x->op->USBCMD,0) & XHCI_USBCMD_HCRST)) break;
    }
}

static void xhci_start(xhci_t* x) {
    u32 cmd = mmio32_read(&x->op->USBCMD,0);
    mmio32_write(&x->op->USBCMD,0, cmd | 1u);
}

static void event_ring_init(xhci_t* x){
    ring_init(&x->evt, XHCI_RING_TRBS);
    ring_alloc_dma(&x->evt);

    xhci_erst_entry_t* erst = (xhci_erst_entry_t*)kzalloc(sizeof(xhci_erst_entry_t));
    erst->ring_base = x->evt.phys;
    erst->ring_size = x->evt.count;

    volatile u8* rt = (volatile u8*)x->rt;
    volatile u64* IMAN   = (volatile u64*)(rt + 0x20);
    volatile u64* IMOD   = (volatile u64*)(rt + 0x28);
    volatile u64* ERSTSZ = (volatile u64*)(rt + 0x30);
    volatile u64* ERSTBA = (volatile u64*)(rt + 0x38);
    volatile u64* ERDP   = (volatile u64*)(rt + 0x40);

    *ERSTSZ = 1;
    *ERSTBA = (u64)(uintptr_t)erst;
    *ERDP   = x->evt.phys | 1ull; // EHB=1
    *IMOD   = 0;
    *IMAN   = 2; // IE
}

static void cmd_ring_init(xhci_t* x){
    ring_init(&x->cmd, XHCI_RING_TRBS);
    ring_alloc_dma(&x->cmd);
    u64 crcr = x->cmd.phys | (x->cmd.cycle & 1);
    write64(&x->op->CRCR, crcr);
}

static void dcbaa_init(xhci_t* x){
    x->dcbaa = (u64*)kalloc_dma(sizeof(u64)*(x->max_slots+1), &x->dcbaa_phys);
    for (u32 i=0;i<=x->max_slots;i++) x->dcbaa[i]=0;
    write64(&x->op->DCBAAP, x->dcbaa_phys);
}

static void xhci_parse_caps(xhci_t* x) {
    u8 caplen = x->cap->CAPLENGTH;
    x->op = (volatile xhci_op_regs_t*)((volatile u8*)x->cap + caplen);
    x->db = (volatile xhci_doorbell_t*)((volatile u8*)x->cap + x->cap->DBOFF);
    x->rt = (volatile xhci_runtime_regs_t*)((volatile u8*)x->cap + x->cap->RTSOFF);
    u32 hcs1 = x->cap->HCSPARAMS1;
    x->max_slots = (hcs1 & 0xFF);
}

static void ports_power_and_reset(xhci_t* x){
    volatile u8* opb = (volatile u8*)x->op;
    x->max_ports = (x->cap->HCSPARAMS1 >> 24) & 0xFF;

    for (u32 p=0;p<x->max_ports;p++) {
        volatile u32* PORTSC = (volatile u32*)(opb + 0x400 + p*0x10);
        *PORTSC |= PORTSC_PP; // zasil
    }
    for (u32 p=0;p<x->max_ports;p++) {
        volatile u32* PORTSC = (volatile u32*)(opb + 0x400 + p*0x10);
        *PORTSC |= PORTSC_PR; // reset
    }
    for (volatile int i=0;i<1000000;i++);
}

static void log_ports(xhci_t* x){
    volatile u8* opb = (volatile u8*)x->op;
    for (u32 p=0;p<x->max_ports;p++) {
        volatile u32* PORTSC = (volatile u32*)(opb + 0x400 + p*0x10);
        u32 v = *PORTSC;
        bool conn = v & PORTSC_CCS;
        bool enabled = v & PORTSC_PED;
        u32 speed = (v & PORTSC_PL_MASK) >> PORTSC_PL_SHIFT;
        klog("xhci","port %u: conn=%d, enabled=%d, speed_code=%u\n", p+1, conn, enabled, speed);
    }
}

// ====== Obsługa eventów (prosty polling) ======
static xhci_trb_t* event_wait_any(xhci_t* x, u32* out_type){
    volatile u8* rt = (volatile u8*)x->rt;
    volatile u64* ERDP = (volatile u64*)(rt + 0x40);
    for (;;) {
        xhci_trb_t* e = &x->evt.trbs[x->evt.idx];
        u8 cycle = e->p3 & 1;
        if (cycle == x->evt.cycle) {
            u32 type = (e->p3 >> 10) & 0x3F;
            // przesuwamy ERDP i wskaźniki
            x->evt.idx++;
            if (x->evt.idx == x->evt.count) { x->evt.idx=0; x->evt.cycle^=1; }
            *ERDP = x->evt.phys + (x->evt.idx * sizeof(xhci_trb_t));
            if (out_type) *out_type = type;
            return e;
        }
    }
}

// ====== Enable Slot + Address Device ======
typedef struct PACKED {
    u8  bmRequestType;
    u8  bRequest;
    u16 wValue;
    u16 wIndex;
    u16 wLength;
} setup_pkt_t;

// Pomocnicze makra do ustawiania pól w kontekstach
static inline u32 slotctx_route(u32 route){ return (route & 0xFFFFF); }
static inline u32 slotctx_speed(u32 speed){ return (speed & 0xF) << 20; }
static inline u32 slotctx_ctx_entries(u32 n){ return (n & 0x1F) << 27; }

static inline u32 epctx_ep_type(u32 t){ return (t & 0x7) << 3; } // 4=Control
static inline u32 epctx_max_packet(u32 mps){ return (mps & 0xFFFF) << 16; }
static inline u32 epctx_dcs(u32 dcs){ return (dcs & 1); }

static u32 mps_for_speed(u32 speed_code){
    // Zgodnie z xHCI/USB: HS=64, SS=512, FS=8, LS=8 (upraszczamy)
    switch(speed_code){
        case 1: return 12;   // LS (często 8) – ale niektóre kontrolery tolerują 8/64; damy 8
        case 2: return 8;    // FS  => 8
        case 3: return 64;   // HS  => 64
        case 4: return 512;  // SS  => 512
        default: return 8;
    }
}

// Tworzy Input Context i Device Context dla slotu 1
static void make_contexts(xhci_t* x, u32 port_speed){
    // Device Context
    x->devctx = (xhci_device_ctx_t*)kalloc_dma(4096, &x->devctx_phys); // 4K align
    // Input Context
    x->inctx  = (xhci_input_ctx_t*)kalloc_dma(4096, &x->inctx_phys);

    // Wpisz Device Context do DCBAA pod slot 1
    x->dcbaa[1] = x->devctx_phys;

    // Wypełnij Input Control Context: dodaj Slot + EP0 (bit0 i bit1)
    x->inctx->icc.drop_flags = 0;
    x->inctx->icc.add_flags  = (1u<<0) | (1u<<1);

    // Slot Context
    u32 mps = mps_for_speed(port_speed);
    x->inctx->slot.dw0 = slotctx_route(0) | slotctx_speed(port_speed) | slotctx_ctx_entries(1); // 1 = co najmniej EP0
    x->inctx->slot.dw1 = 0;
    x->inctx->slot.dw2 = 0;
    x->inctx->slot.dw3 = 0;
    x->inctx->slot.dw4 = 0;
    x->inctx->slot.dw5 = 0;
    x->inctx->slot.dw6 = 0;
    x->inctx->slot.dw7 = 0;

    // EP0 Context – typ Control, TR Dequeue, MPS
    // TRDP (64-bit) w dw6/dw7: bity 3..63 adres, bity 0..2 = DCS i reszta
    // Zbudujemy ring dla EP0 i ustawimy go tutaj.
    ring_init(&x->ep0_ring, 32);
    ring_alloc_dma(&x->ep0_ring);
    x->ep0_dq_ptr = x->ep0_ring.phys;

    x->inctx->ep0.dw0 = epctx_ep_type(4) | epctx_dcs(1); // Control, DCS=1
    x->inctx->ep0.dw1 = 0; // Max ESIT payload (nie dotyczy control)
    x->inctx->ep0.dw2 = 0;
    x->inctx->ep0.dw3 = 0;
    x->inctx->ep0.dw4 = 0;
    x->inctx->ep0.dw5 = 0;
    // Dequeue pointer: adres >> 4, bity 3.. n; ustaw DCS w dw0
    u64 dq = x->ep0_dq_ptr;
    x->inctx->ep0.dw6 = (u32)((dq >> 4) & 0xFFFFFFFF);
    x->inctx->ep0.dw7 = (u32)(dq >> 36);

    // MPS w dw1/dw2 zależnie od kontrolera; częsty układ w xHCI:
    // ustawia się w dw1 Max Packet Size (bits 16..31) – użyjemy tak:
    x->inctx->ep0.dw1 |= epctx_max_packet(mps);
}

// Wysyła komendę Enable Slot i zwraca SlotID z Command Completion
static u32 cmd_enable_slot(xhci_t* x){
    ring_push(&x->cmd, 0,0,0, (TRB_ENABLE_SLOT<<10));
    cmd_ring_doorbell(x);

    for (;;) {
        u32 type=0; xhci_trb_t* e = event_wait_any(x,&type);
        if (type==TRB_CMD_COMPLETION){
            u32 slotid = (e->p2 >> 24) & 0xFF;
            return slotid;
        }
    }
}

// Wysyła Address Device wskazując Input Context; BSR=0
static bool cmd_address_device(xhci_t* x, u32 slotid){
    u32 p0 = (u32)(x->inctx_phys & 0xFFFFFFFF);
    u32 p1 = (u32)(x->inctx_phys >> 32);
    u32 p2 = 0; // BSR=0
    u32 p3 = (TRB_ADDRESS_DEV<<10) | TRB_IOC;
    ring_push(&x->cmd, p0,p1,p2,p3);
    cmd_ring_doorbell(x);

    for (;;) {
        u32 type=0; xhci_trb_t* e = event_wait_any(x,&type);
        if (type==TRB_CMD_COMPLETION){
            // Sprawdź kod statusu w e->p2 (bity 0..23) jeśli chcesz
            return true;
        }
    }
}

// ====== Transfer kontrolny GET_DESCRIPTOR(DEVICE) po EP0 ======
static void db_ring_ep0(xhci_t* x, u32 slotid){
    // Doorbell dla slotu N: zapis do DB[N] z targetem endpointu (EP0 = 1)
    volatile u32* db = &((&x->db->DB)[slotid]); // offset w słowach
    *db = 1; // Target=1 => EP0
}

// Składa 3 TRB: Setup (IDT), Data IN, Status OUT; czeka na Transfer Event
static bool ep0_get_device_descriptor(xhci_t* x, u32 slotid, usb_device_desc_t* out){
    // 1) Setup packet (natychmiastowe dane IDT)
    setup_pkt_t sp;
    sp.bmRequestType = 0x80; // IN, std, device
    sp.bRequest      = USB_REQ_GET_DESCRIPTOR;
    sp.wValue        = (USB_DT_DEVICE<<8) | 0; // index 0
    sp.wIndex        = 0;
    sp.wLength       = sizeof(usb_device_desc_t);

    // Skopiuj 8 bajtów setup do dwóch u32
    u32 s0 = ((u32)sp.bmRequestType) | ((u32)sp.bRequest<<8) | ((u32)sp.wValue<<16);
    u32 s1 = ((u32)sp.wIndex) | ((u32)sp.wLength<<16);

    // Bufor danych (18B) – DMA
    u64 dbuf_phys=0;
    usb_device_desc_t* dbuf = (usb_device_desc_t*)kalloc_dma(64, &dbuf_phys);

    // TRB Setup Stage
    ring_push(&x->ep0_ring,
        s0, s1, 8,                                 // p2: len=8 bajtów setup
        (TRB_SETUP_STAGE<<10) | TRB_IDT | TRB_IOC  // IDT + IOC
    );

    // TRB Data Stage (IN)
    ring_push(&x->ep0_ring,
        (u32)(dbuf_phys & 0xFFFFFFFF),
        (u32)(dbuf_phys >> 32),
        sizeof(usb_device_desc_t),
        (TRB_DATA_STAGE<<10) | TRB_DIR_IN | TRB_IOC
    );

    // TRB Status Stage (OUT) – dla transferu IN status jest OUT
    ring_push(&x->ep0_ring,
        0,0,0,
        (TRB_STATUS_STAGE<<10) | TRB_IOC
    );

    // Zadzwoń dzwonkiem endpointu 0 w slocie
    db_ring_ep0(x, slotid);

    // Czekaj na Transfer Event (bardzo prosto)
    bool got_data=false;
    for (int ev=0; ev<10; ++ev){
        u32 type=0; xhci_trb_t* e = event_wait_any(x,&type);
        if (type==TRB_TRANSFER_EVENT){
            got_data=true; break;
        } else if (type==TRB_PORT_STATUS || type==TRB_CMD_COMPLETION){
            // inne eventy olewamy w tym demie
        }
    }
    if (!got_data) return false;

    // Skopiuj wynik
    *out = *dbuf;
    return true;
}

// ====== Bring-up jednego urządzenia z portu 1 (demo) ======
static bool enumerate_first_device(xhci_t* x){
    // Załóżmy urządzenie na porcie 1 (QEMU: usb-kbd / usb-tablet)
    volatile u8* opb = (volatile u8*)x->op;
    volatile u32* PORTSC1 = (volatile u32*)(opb + 0x400 + 0*0x10);
    u32 v=*PORTSC1;
    if (!(v & PORTSC_CCS)) { klog("xhci","Port 1: brak urządzenia.\n"); return false; }
    u32 speed = (v & PORTSC_PL_MASK) >> PORTSC_PL_SHIFT;

    // 1) Enable Slot
    u32 slotid = cmd_enable_slot(x);
    if (slotid==0){ klog("xhci","Enable Slot nie zwrócił slotu.\n"); return false; }

    // 2) Przygotuj konteksty i Address Device
    make_contexts(x, speed);
    if (!cmd_address_device(x, slotid)){ klog("xhci","Address Device nie powiodło się.\n"); return false; }

    // 3) GET_DESCRIPTOR(DEVICE)
    usb_device_desc_t devd;
    if (!ep0_get_device_descriptor(x, slotid, &devd)){
        klog("xhci","GET_DESCRIPTOR nie powiódł się.\n");
        return false;
    }
    klog("xhci","Device: VID=%04x PID=%04x, bMaxPacketSize0=%u, bNumConfigs=%u\n",
         devd.idVendor, devd.idProduct, devd.bMaxPacketSize0, devd.bNumConfigurations);
    return true;
}

// ========= Public: probe + init =========
static void xhci_bringup_at(u32 bus,u32 dev,u32 fun) {
    u64 bar = pci_get_bar_mmio(bus,dev,fun,XHCI_PCI_BAR);
    if (!bar) return;

    xhci_t* x = (xhci_t*)kzalloc(sizeof(xhci_t));
    x->mmio_base = map_mmio(bar,&x->mmio_len);
    x->cap = (volatile xhci_cap_regs_t*)x->mmio_base;

    xhci_parse_caps(x);
    klog("xhci","HCIVERSION=%04x max_slots=%u DBOFF=0x%x RTSOFF=0x%x\n",
         x->cap->HCIVERSION, x->max_slots, x->cap->DBOFF, x->cap->RTSOFF);

    xhci_reset(x);
    cmd_ring_init(x);
    event_ring_init(x);
    dcbaa_init(x);

    // Pozwól na maksymalną liczbę slotów
    mmio32_write(&x->op->CONFIG, 0, x->max_slots);

    xhci_start(x);

    ports_power_and_reset(x);
    log_ports(x);

    // Enumeruj demo: port 1
    enumerate_first_device(x);

    // Zarejestruj HC w warstwie USB core
    usb_hc_t* hc = (usb_hc_t*)kzalloc(sizeof(usb_hc_t));
    hc->kind = USB_HCI_XHCI;
    hc->impl = x;
    usb_register_hc(hc);

    klog("xhci","controller @ %02x:%02x.%u online.\n", bus,dev,fun);
    klog("xhci","Demo gotowe: Address Device + GET_DESCRIPTOR wykonane dla portu 1 (jeśli obecny).\n");
}

void xhci_probe_all(void) {
    for (u32 bus=0; bus<256; ++bus)
    for (u32 dev=0; dev<32; ++dev)
    for (u32 fun=0; fun<8; ++fun) {
        u32 vendor_device = pci_read32(bus,dev,fun,0x00);
        if (vendor_device == 0xFFFFFFFF) continue;
        if (pci_match_xhci(bus,dev,fun)) xhci_bringup_at(bus,dev,fun);
    }
}