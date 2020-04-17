//  Copyright 2016-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  fb_smo_print.c
//
//  Created by Gilles Boccon-Gibod on 11/4/16.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fb_smo.h"
#include "fb_smo_serialization.h"

#define FB_SMO_PRINT_MAX_OFFSET 64

static void
PrintSmo(Fb_Smo* smo, unsigned int offset, unsigned int indent_first_line)
{
    Fb_Smo* child;
    char prefix[2*FB_SMO_PRINT_MAX_OFFSET+1];

    if (offset > FB_SMO_PRINT_MAX_OFFSET) {
        offset = FB_SMO_PRINT_MAX_OFFSET;
    }
    memset(prefix, ' ', 2*offset);
    prefix[2*offset] = '\0';

    switch (Fb_Smo_GetType(smo)) {
      case FB_SMO_TYPE_ARRAY:
        child = Fb_Smo_GetFirstChild(smo);
        printf("%s[\n", indent_first_line?prefix:"");
        while (child) {
            PrintSmo(child, offset+1, 1);
            if (Fb_Smo_GetNext(child)) {
                printf(",");
            }
            printf("\n");
            child = Fb_Smo_GetNext(child);
        }
        printf("%s]", prefix);
        break;

      case FB_SMO_TYPE_OBJECT:
        child = Fb_Smo_GetFirstChild(smo);
        printf("%s{\n", indent_first_line?prefix:"");
        while (child) {
            printf("  %s\"%s\" = ", prefix, Fb_Smo_GetName(child));
            PrintSmo(child, offset+1, 0);
            if (Fb_Smo_GetNext(child)) {
                printf(",");
            }
            printf("\n");
            child = Fb_Smo_GetNext(child);
        }
        printf("%s}", prefix);
        break;

      case FB_SMO_TYPE_INTEGER:
        printf("%s%" PRIi64, indent_first_line?prefix:"", Fb_Smo_GetValueAsInteger(smo));
        break;

      case FB_SMO_TYPE_FLOAT:
        printf("%s%f", indent_first_line?prefix:"", Fb_Smo_GetValueAsFloat(smo));
        break;

      case FB_SMO_TYPE_STRING:
        printf("%s\"%s\"", indent_first_line?prefix:"", Fb_Smo_GetValueAsString(smo));
        break;

      case FB_SMO_TYPE_BYTES:
        printf("%s(", indent_first_line?prefix:"");
        {
            unsigned int bytes_size = 0;
            const uint8_t* bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
            unsigned int i;
            for (i=0; i<bytes_size; i++) {
                printf("%02x", bytes[i]);
            }
        }
        printf(")");
        break;

      case FB_SMO_TYPE_SYMBOL:
        printf("%s", indent_first_line?prefix:"");
        switch (Fb_Smo_GetValueAsSymbol(smo)) {
          case FB_SMO_SYMBOL_NULL:
            printf("null");
            break;

          case FB_SMO_SYMBOL_TRUE:
            printf("true");
            break;

          case FB_SMO_SYMBOL_FALSE:
            printf("false");
            break;

          case FB_SMO_SYMBOL_UNDEFINED:
            printf("undefined");
            break;
        }
        break;
    }

    return;
}

static void
DeserializeAndPrint(const uint8_t* buffer, unsigned int buffer_size)
{
    Fb_Smo* smo = NULL;
    int result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, buffer_size, &smo);

    if (result != FB_SMO_SUCCESS) {
        fprintf(stderr, "ERROR: Fb_Smo_Deserialize failed (%d)\n", result);
        return;
    }

    if (smo) {
        PrintSmo(smo, 0, 1);
        printf("\n");
    }
}

int
main(int argc, const char * argv[]) {
    FILE* input = NULL;
    uint8_t* buffer = NULL;
    unsigned int buffer_size = 0;

    if (argc != 2) {
        fprintf(stderr, "ERROR: input filename expected\n");
        return 1;
    }

    input = fopen(argv[1], "rb");
    if (!input) {
        fprintf(stderr, "ERROR: cannot open %s\n", argv[1]);
        return 1;
    }

    if (fseek(input, 0, SEEK_END) == 0) {
        buffer_size = (unsigned int)ftell(input);
        fseek(input, 0, SEEK_SET);
    }

    buffer = (uint8_t*)malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "ERROR: out of memory\n");
        return 1;
    }

    if (fread(buffer, buffer_size, 1, input) != 1) {
        fprintf(stderr, "ERROR: failed to read input\n");
        return 1;
    }

    DeserializeAndPrint(buffer, buffer_size);

    if (buffer) free(buffer);

    return 0;
}
