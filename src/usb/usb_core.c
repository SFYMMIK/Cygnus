/*
 * [Cygnus] - [src/usb_core.c]
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
#include "../../inc/usb/usb_core.h"
#include "../../inc/usb/usb_types.h"

static usb_hc_t* h_usb_hc_list = NULL;

void usb_register_hc(usb_hc_t* hc) {
  hc->next = g_usb_hc_list;
  g_usb_hc_list = hc;
  klog("usb", "registered HC kind=%d impl=%p\n", hc->kind, hc->impl);
}

void usb_core_init(void) {
  klog("usb", "USB core online. Waiting for HCs to register...\n");
 // W sekwencji boot: wywołaj xhci_probe_all(); ehci_probe_all(); później enumeracja.
}