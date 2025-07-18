/*
 * [Cygnus] - [src/sbin/cat.c]
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
#include "../fat16.h"
#include "../../inc/std.h"

int cat_main(int argc, char **argv) {
    if (argc < 2) { print("cat: need file\n"); return 1; }
    int fd = fat16_open(argv[1], 0);
    if (fd < 0) { print("cat: can't open file\n"); return 1; }
    char buf[256]; int n;
    while ((n = fat16_read(fd, buf, 256)) > 0)
        for (int i = 0; i < n; i++) putchar(buf[i]);
    fat16_close(fd);
    return 0;
}