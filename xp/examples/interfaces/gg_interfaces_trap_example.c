/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 209-11-25
 *
 * @details
 *
 * Usage examples for interface traps.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xp/common/gg_common.h"


GG_DECLARE_INTERFACE(Interface1) {
    int (*Method1)(const Interface1* self, int x);
    void (*Method2)(const Interface1* self);
};

GG_DECLARE_INTERFACE(Interface2) {
    int (*Method1)(const Interface2* self, int x, int y);
    void (*Method2)(const Interface2* self, int a);
};


typedef struct {
    GG_IMPLEMENTS(Interface1);
    GG_IMPLEMENTS(Interface2);

    int a;
    const char* b;
} Foobar;

//---------------------------------------------------------------------------------------
static int Interface1_Method1(const Interface1* self, int x) {
    return GG_INTERFACE(self)->Method1(self, x);
}
static void Interface1_Method2(const Interface1* self) {
    GG_INTERFACE(self)->Method2(self);
}
static int Interface2_Method1(const Interface2* self, int x, int y) {
    return GG_INTERFACE(self)->Method1(self, x, y);
}
static void Interface2_Method2(const Interface2* self, int a) {
    GG_INTERFACE(self)->Method2(self, a);
}

//---------------------------------------------------------------------------------------
static int Foobar_Interface1_Method1(const Interface1* _self, int x) {
    Foobar* self = GG_SELF(Foobar, Interface1);
    printf("Interface1_Method1 - x=%d [self.a=%d, self.b=%s]\n", x, self->a, self->b);
    return 999;
}

static void Foobar_Interface1_Method2(const Interface1* _self) {
    Foobar* self = GG_SELF(Foobar, Interface1);
    printf("Interface1_Method2 [self.a=%d, self.b=%s]\n", self->a, self->b);
}

static int Foobar_Interface2_Method1(const Interface2* _self, int x, int y) {
    Foobar* self = GG_SELF(Foobar, Interface2);
    printf("Interface2_Method1 - x=%d, y=%d [self.a=%d, self.b=%s]\n", x, y, self->a, self->b);
    return 999;
}

static void Foobar_Interface2_Method2(const Interface2* _self, int a) {
    Foobar* self = GG_SELF(Foobar, Interface2);
    printf("Interface2_Method2 - a=%d [self.a=%d, self.b=%s]\n", a, self->a, self->b);
}

//---------------------------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(Foobar, Interface1) {
    Foobar_Interface1_Method1,
    Foobar_Interface1_Method2
};

GG_IMPLEMENT_INTERFACE(Foobar, Interface2) {
    Foobar_Interface2_Method1,
    Foobar_Interface2_Method2
};

//---------------------------------------------------------------------------------------
static void Foobar_Create(Foobar** foobar) {
    *foobar = (Foobar*)GG_AllocateMemory(sizeof(Foobar));
    (*foobar)->a = 222;
    (*foobar)->b = "foo";

    GG_SET_INTERFACE(*foobar, Foobar, Interface1);
    GG_SET_INTERFACE(*foobar, Foobar, Interface2);
}

static void Foobar_Destroy(Foobar* self) {
    GG_ClearAndFreeObject(self, 2);
}

//---------------------------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("Intercace Trap Example\n");

    // Instantiate a new Foobar object
    Foobar* foobar = NULL;
    Foobar_Create(&foobar);

    // Call some methods
    int x = Interface1_Method1(GG_CAST(foobar, Interface1), 333);
    Interface1_Method2(GG_CAST(foobar, Interface1));
    Interface2_Method1(GG_CAST(foobar, Interface2), x, 567);
    Interface2_Method2(GG_CAST(foobar, Interface2), x+1);

    // Destroy the object
    Foobar_Destroy(foobar);

    // Call the destroyed object (should log and crash)
    Interface2_Method2(GG_CAST(foobar, Interface2), 123);

    return 0;
}
