/*
 * [Cygnus] - [src/shell.c]
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
#include "inc/fat16.h"
#include "inc/std.h"
#include <string.h>

extern int ls_main(int, char **);
extern int cat_main(int, char **);
// Add other commands as externs

struct {
    const char *name;
    int (*main)(int, char **);
} commands[] = {
    { "ls", ls_main },
    { "cat", cat_main },
    // add more commands here
};

void shell() {
    char line[128];
    print("Welcome to Cygnus!\n");
    while (1) {
        print("$ ");
        gets(line, sizeof(line));
        char *argv[8]; int argc = 0;
        char *p = strtok(line, " ");
        while (p && argc < 8) { argv[argc++] = p; p = strtok(0, " "); }
        if (argc == 0) continue;
        int found = 0;
        for (unsigned i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                commands[i].main(argc, argv);
                found = 1; break;
            }
        }
        if (!found) print("Unknown command\n");
    }
}