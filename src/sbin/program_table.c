/*
 * [Cygnus] - [src/sbin/program_table.c]
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
#include <string.h>
typedef int (*prog_main_t)(int, char **);

int ls_main(int, char **);
int cat_main(int, char **);
int mkdir_main(int, char **);
int touch_main(int, char **);
int rm_main(int, char **);
int cp_main(int, char **);
int mv_main(int, char **);
int editor_main(int, char **);

struct program_entry {
    const char *name;
    prog_main_t fn;
} programs[] = {
    {"ls", ls_main},
    {"cat", cat_main},
    {"mkdir", mkdir_main},
    {"touch", touch_main},
    {"rm", rm_main},
    {"cp", cp_main},
    {"mv", mv_main},
    {"editor", editor_main},
    {NULL, NULL}
};

prog_main_t find_program(const char *name) {
    for (int i = 0; programs[i].name; ++i)
        if (!strcmp(programs[i].name, name))
            return programs[i].fn;
    return NULL;
}