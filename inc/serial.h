/*
 * [Cygnus] - [inc/serial.h]
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
#ifndef CYGNUS_SERIAL_H
#define CYGNUS_SERIAL_H

#include <stdint.h>

/* Domyślna baza dla COM1 */
#define COM1_BASE 0x3F8

/* Inicjalizacja UART:
 * - ustawiamy bazę portu,
 * - konfigurujemy 115200 8N1,
 * - czyścimy FIFO.
 */
void serial_init(uint16_t base);

/* Jeżeli mamy kilka portów, możemy zmienić bazę „w locie”. */
void serial_set_base(uint16_t base);

/* Wypisanie jednego znaku (blokujące).
 * Dla wygody wysyłamy CR przed LF (tj. '\r' przed '\n').
 */
void serial_write_char(char c);

/* Wypisanie łańcucha znaków (blokujące). */
void serial_write(const char *s);

/* Czy jest znak do odczytu? Zwracamy !=0, jeżeli tak. */
int  serial_can_read(void);

/* Odczyt jednego znaku (blokujący). */
char serial_read(void);

#endif /* CYGNUS_SERIAL_H */
