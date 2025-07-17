/*
 * [Cygnus] - [src/sbin/cp.c]
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
#include "../std.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        print("cp: missing file operands\n");
        return 1;
    }
    if (fat16_cp(argv[1], argv[2]) != 0)
        print("cp: failed to copy\n");
    return 0;
}