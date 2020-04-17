//  Copyright 2016-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  fb_smo_parse.c
//
//  Created by Gilles Boccon-Gibod
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fb_smo.h"
#include "fb_smo_cbor.h"

#define FB_SMO_PRINT_MAX_OFFSET 64

typedef struct ParsePrinterContext ParsePrinterContext;
struct ParsePrinterContext {
    char                 type;
    unsigned int         entry_count;
    int                  have_name;
    ParsePrinterContext* parent;
};

typedef struct {
    Fb_Smo_CBOR_ParserListener parser_listener;
    char                       prefix[2*FB_SMO_PRINT_MAX_OFFSET+1];
    ParsePrinterContext*       context;
    unsigned int               indent_offset;
} ParsePrinter;

static void
ParsePrinter_PushContext(ParsePrinter* self, char type, unsigned int entry_count)
{
    unsigned int indent_offset = ++self->indent_offset;
    ParsePrinterContext* new_context = (ParsePrinterContext*)malloc(sizeof(ParsePrinterContext));
    if (new_context == NULL) {
        fprintf(stderr, "ERROR: out of memory\n");
        exit(1);
    }

    if (indent_offset > FB_SMO_PRINT_MAX_OFFSET) {
        indent_offset = FB_SMO_PRINT_MAX_OFFSET;
    }
    memset(self->prefix, ' ', 2*indent_offset);
    self->prefix[2*indent_offset] = '\0';

    new_context->parent      = self->context;
    new_context->entry_count = entry_count;
    new_context->type        = type;
    new_context->have_name   = 0;

    self->context            = new_context;
}

static void
ParsePrinter_PopContext(ParsePrinter* self)
{
    char c = self->context->type;
    ParsePrinterContext* context = self->context;

    self->context = self->context->parent;
    free(context);

    if (self->indent_offset) {
        --self->indent_offset;
        memset(self->prefix, ' ', 2*self->indent_offset);
        self->prefix[2*self->indent_offset] = '\0';
    }

    printf("%s%c\n", self->prefix, c);
}

static void
ParsePrinter_OnEntryComplete(ParsePrinter* self)
{
    if (self->context) {
        if (self->context->type == '}') {
            if (!self->context->have_name) {
                self->context->have_name = 1;
                printf("%s =\n", self->prefix);
                return;
            } else {
                self->context->have_name = 0;
            }
        }
        if (self->context->entry_count) {
            if (--self->context->entry_count == 0) {
                ParsePrinter_PopContext(self);
            }
        }
    }
}

static void
ParsePrinter_OnInteger(Fb_Smo_CBOR_ParserListener* _self, int64_t value)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s%lld\n", self->prefix, value);
    ParsePrinter_OnEntryComplete(self);
}

static void
ParsePrinter_OnFloat(Fb_Smo_CBOR_ParserListener* _self, double value)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s%f\n", self->prefix, value);
    ParsePrinter_OnEntryComplete(self);
}

static void
ParsePrinter_OnSymbol(Fb_Smo_CBOR_ParserListener* _self, Fb_SmoSymbol value)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s", self->prefix);
    switch (value) {
      case FB_SMO_SYMBOL_NULL:
        printf("null\n");
        break;

      case FB_SMO_SYMBOL_TRUE:
        printf("true\n");
        break;

      case FB_SMO_SYMBOL_FALSE:
        printf("false\n");
        break;

      case FB_SMO_SYMBOL_UNDEFINED:
        printf("undefined\n");
        break;
    }
    ParsePrinter_OnEntryComplete(self);
}

static void
ParsePrinter_OnString(Fb_Smo_CBOR_ParserListener* _self, const char* value, unsigned int value_length)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    char* terminated_string = strndup(value, value_length);
    printf("%s\"%s\"\n", self->prefix, terminated_string);
    free(terminated_string);
    ParsePrinter_OnEntryComplete(self);
}

static void
ParsePrinter_OnBytes(Fb_Smo_CBOR_ParserListener* _self, const uint8_t* value, unsigned int value_length)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s(", self->prefix);
    {
        unsigned int i;
        for (i=0; i<value_length; i++) {
            printf("%02x", value[i]);
        }
    }
    printf(")\n");
    ParsePrinter_OnEntryComplete(self);
}

static void
ParsePrinter_OnArray(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s[\n", self->prefix);
    if (entry_count) {
        ParsePrinter_PushContext(self, ']', entry_count);
    } else {
        printf("%s]\n", self->prefix);
        ParsePrinter_OnEntryComplete(self);
    }
}

static void
ParsePrinter_OnObject(Fb_Smo_CBOR_ParserListener* _self, unsigned int entry_count)
{
    ParsePrinter* self = (ParsePrinter*)_self;
    printf("%s{\n", self->prefix);
    if (entry_count) {
        ParsePrinter_PushContext(self, '}', entry_count);
    } else {
        printf("%s}\n", self->prefix);
        ParsePrinter_OnEntryComplete(self);
    }
}

static void
ParseAndPrint(const uint8_t* buffer, unsigned int buffer_size)
{
    ParsePrinter printer = {
        {
            ParsePrinter_OnInteger,
            ParsePrinter_OnFloat,
            ParsePrinter_OnSymbol,
            ParsePrinter_OnString,
            ParsePrinter_OnBytes,
            ParsePrinter_OnObject,
            ParsePrinter_OnArray
        }
    };
    unsigned int bytes_left = buffer_size;

    printer.indent_offset = 0;
    printer.context = NULL;
    printer.prefix[0] = '\0';

    while (bytes_left) {
        int result = Fb_Smo_Parse_CBOR(&printer.parser_listener, buffer+buffer_size-bytes_left, &bytes_left);
        if (result != FB_SMO_SUCCESS) {
            fprintf(stderr, "ERROR: Fb_Smo_Parse_CBOR returned %d\n", result);
            break;
        }
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

    ParseAndPrint(buffer, buffer_size);

    if (buffer) free(buffer);

    return 0;
}
