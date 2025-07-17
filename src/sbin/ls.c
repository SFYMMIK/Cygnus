/*
 * [Cygnus] - [src/sbin/ls.c]
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
#include "../../inc/fat16.h"
#include "../../inc/std.h"

int ls_main(int argc, char **argv) {
    char name[64]; int is_dir;
    int dh = fat16_opendir("/");
    if (dh < 0) { print("ls: can't open /\n"); return 1; }
    while (fat16_readdir(dh, name, &is_dir) == 0) {
        print(name);
        if (is_dir) print("/");
        print(" ");
    }
    print("\n");
    fat16_closedir(dh);
    return 0;
}