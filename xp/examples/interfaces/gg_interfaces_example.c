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
 * @date 2017-09-18
 *
 * @details
 *
 * Usage examples for the interfaces macros.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_timer.h"

//---------------------------------------------------------------------------------------
// 1. Simple example of an concrete, non-virtual, non-polymorphic object-oriented interface

//
// start of public header definitions for Example 1
//
typedef struct GG_Accumulator GG_Accumulator;
GG_Result GG_Accumulator_Create(int constant, GG_Accumulator** accumulator);
void GG_Accumulator_Destroy(GG_Accumulator* self);
void GG_Accumulator_Add(GG_Accumulator* self, int value);
int GG_Accumulator_GetValue(const GG_Accumulator* self);
//
// end of public header definitions for Example 1
//

//
// start of implementation for Example 1
//
struct GG_Accumulator {
    int value;
};

GG_Result
GG_Accumulator_Create(int initial_value, GG_Accumulator** accumulator)
{
    // allocate an instance (in this example we allocate from the heap)
    *accumulator = (GG_Accumulator*)malloc(sizeof(GG_Accumulator));
    if (*accumulator == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // init the instance data
    (*accumulator)->value = initial_value;

    return GG_SUCCESS;
}

void
GG_Accumulator_Destroy(GG_Accumulator* self)
{
    if (self) {
        free(self);
    }
}

void
GG_Accumulator_Add(GG_Accumulator* self, int value)
{
    self->value += value;
}

int
GG_Accumulator_GetValue(const GG_Accumulator* self)
{
    return self->value;
}
//
// end of implementation for Example 1
//

//
// usage example for Example 1
//
static void
Example1()
{
    printf("* Example 1\n");

    // instantiate an accumulator object and call it
    GG_Accumulator* accumulator = NULL;
    printf("Creating accumulator with initial value = 2\n");
    GG_Result result = GG_Accumulator_Create(2, &accumulator);
    if (GG_FAILED(result)) return;

    printf("Adding 5\n");
    GG_Accumulator_Add(accumulator, 5);
    printf("New Value = %d\n", GG_Accumulator_GetValue(accumulator));

    printf("Adding 6\n");
    GG_Accumulator_Add(accumulator, 6);
    printf("New Value = %d\n", GG_Accumulator_GetValue(accumulator));

    // done
    GG_Accumulator_Destroy(accumulator);
}

//---------------------------------------------------------------------------------------
// 2. Example with a virtual interface

//
// start of public header definitions for example 2
//
GG_DECLARE_INTERFACE(GG_Shape) {
    void   (*ToString)(const GG_Shape* self, char* buffer, unsigned int buffer_size);
    double (*GetArea)(const GG_Shape* self);
    void   (*Destroy)(GG_Shape* self);
};

void   GG_Shape_ToString(const GG_Shape* self, char* buffer, unsigned int buffer_size);
double GG_Shape_GetArea(const GG_Shape* self);
void   GG_Shape_Destroy(GG_Shape* self);

GG_DECLARE_INTERFACE(GG_ShapeVisitor) {
    void (*Visit)(GG_ShapeVisitor* self, const GG_Shape* shape);
};

void GG_ShapeVisitor_Visit(GG_ShapeVisitor* self, const GG_Shape* shape);

typedef struct GG_Rectangle GG_Rectangle;
GG_Result GG_Rectangle_Create(unsigned int width, unsigned int height, GG_Rectangle** rectangle);

typedef struct GG_Circle GG_Circle;
GG_Result GG_Circle_Create(unsigned int radius, GG_Circle** circle);

//
// end of public header definitions for example 2
//

//
// start of implementation for Example 2
//
void
GG_Shape_ToString(const GG_Shape* self, char* buffer, unsigned int buffer_size)
{
    GG_INTERFACE(self)->ToString(self, buffer, buffer_size);
}

double
GG_Shape_GetArea(const GG_Shape* self)
{
    return GG_INTERFACE(self)->GetArea(self);
}

void
GG_Shape_Destroy(GG_Shape* self)
{
    GG_INTERFACE(self)->Destroy(self);
}

void
GG_ShapeVisitor_Visit(GG_ShapeVisitor* self, const GG_Shape* shape)
{
    GG_INTERFACE(self)->Visit(self, shape);
}

struct GG_Rectangle {
    GG_IMPLEMENTS(GG_Shape);

    unsigned int width;
    unsigned int height;
};

static void
GG_Rectangle_ToString(const GG_Shape* _self, char* buffer, unsigned int buffer_size)
{
    GG_Rectangle* self = GG_SELF(GG_Rectangle, GG_Shape);

    snprintf(buffer, buffer_size, "Rectangle, width=%d, height=%d", self->width, self->height);
}

static double
GG_Rectangle_GetArea(const GG_Shape* _self)
{
    GG_Rectangle* self = GG_SELF(GG_Rectangle, GG_Shape);

    return (double)self->width * (double)self->height;
}

static void
GG_Rectangle_Destroy(GG_Shape* self)
{
    if (self) {
        free(self);
    }
}

GG_IMPLEMENT_INTERFACE(GG_Rectangle, GG_Shape) {
    GG_Rectangle_ToString,
    GG_Rectangle_GetArea,
    GG_Rectangle_Destroy
};

GG_Result
GG_Rectangle_Create(unsigned int width, unsigned int height, GG_Rectangle** rectangle)
{
    // allocate an instance (in this example we allocate from the heap)
    *rectangle = (GG_Rectangle*)malloc(sizeof(GG_Rectangle));

    // initialize the instance
    (*rectangle)->width  = width;
    (*rectangle)->height = height;

    // setup the function table
    GG_SET_INTERFACE(*rectangle, GG_Rectangle, GG_Shape);

    return GG_SUCCESS;
}

struct GG_Circle {
    GG_IMPLEMENTS(GG_Shape);

    unsigned int radius;
};

static void
GG_Circle_ToString(const GG_Shape* _self, char* buffer, unsigned int buffer_size)
{
    GG_Circle* self = GG_SELF(GG_Circle, GG_Shape);

    snprintf(buffer, buffer_size, "Circle, radius=%d", self->radius);
}

static double
GG_Circle_GetArea(const GG_Shape* _self)
{
    GG_Circle* self = GG_SELF(GG_Circle, GG_Shape);

    return (double)self->radius * (double)self->radius * 3.14159265359;
}

static void
GG_Circle_Destroy(GG_Shape* self)
{
    if (self) {
        free(self);
    }
}

GG_IMPLEMENT_INTERFACE(GG_Circle, GG_Shape) {
    GG_Circle_ToString,
    GG_Circle_GetArea,
    GG_Circle_Destroy
};

GG_Result
GG_Circle_Create(unsigned int radius, GG_Circle** circle)
{
    // allocate an instance (in this example we allocate from the heap)
    *circle = (GG_Circle*)malloc(sizeof(GG_Circle));

    // initialize the instance
    (*circle)->radius = radius;

    // setup the function table
    GG_SET_INTERFACE(*circle, GG_Circle, GG_Shape);

    return GG_SUCCESS;
}

//
// end of implementation for Example 2
//

//
// start of usage example for Example 2.
//
static void
PrintShape(const GG_Shape* shape)
{
    char string[256];
    GG_Shape_ToString(shape, string, sizeof(string));
    double area = GG_Shape_GetArea(shape);
    printf("+++ Shape: %s, area=%f\n", string, area);
}

static void
VisitShapes(const GG_Shape** shapes, unsigned int shape_count, GG_ShapeVisitor* visitor)
{
    for (unsigned int i=0; i<shape_count; i++) {
        char string[256];
        GG_Shape_ToString(shapes[i], string, sizeof(string));
            printf("--- visiting shape: %s\n", string);
        GG_ShapeVisitor_Visit(visitor, shapes[i]);
    }
}

typedef struct {
    GG_IMPLEMENTS(GG_ShapeVisitor);

    unsigned int visit_count;
} SimpleVisitor;

static void
SimpleVisitor_Visit(GG_ShapeVisitor* _self, const GG_Shape* shape)
{
    SimpleVisitor* self = GG_SELF(SimpleVisitor, GG_ShapeVisitor);

    ++self->visit_count;
    printf("shape %d - area = %f\n", self->visit_count, GG_Shape_GetArea(shape));
}

GG_IMPLEMENT_INTERFACE(SimpleVisitor, GG_ShapeVisitor) {
    SimpleVisitor_Visit
};

static void
Example2()
{
    printf("* Example 2\n");

    GG_Rectangle* rectangle = NULL;
    GG_Result result = GG_Rectangle_Create(3, 4, &rectangle);
    GG_ASSERT(GG_SUCCEEDED(result));

    PrintShape(GG_CAST(rectangle, GG_Shape));

    GG_Circle* circle = NULL;
    result = GG_Circle_Create(5, &circle);
    GG_ASSERT(GG_SUCCEEDED(result));

    PrintShape(GG_CAST(circle, GG_Shape));

    // init a visitor
    SimpleVisitor visitor;
    GG_SET_INTERFACE(&visitor, SimpleVisitor, GG_ShapeVisitor);
    visitor.visit_count = 0;

    // visit the shapes
    const GG_Shape* shapes[2] = {GG_CAST(rectangle, GG_Shape), GG_CAST(circle, GG_Shape)};
    VisitShapes(shapes, 2, GG_CAST(&visitor, GG_ShapeVisitor));

    // cleanup
    GG_Shape_Destroy(GG_CAST(rectangle, GG_Shape));
    GG_Shape_Destroy(GG_CAST(circle, GG_Shape));
}

//---------------------------------------------------------------------------------------
// 3. More advanced example where an object embeds objects that implement interfaces
typedef struct {
    GG_IMPLEMENTS(GG_ShapeVisitor);
} AreaVisitor;

typedef struct {
    GG_IMPLEMENTS(GG_ShapeVisitor);
} ToStringVisitor;

typedef struct {
    AreaVisitor     area_visitor;
    ToStringVisitor to_string_visitor;

    const char* prefix;
} MultiVisitor;

static void
AreaVisitor_Visit(GG_ShapeVisitor* _self, const GG_Shape* shape)
{
    MultiVisitor* self = GG_SELF_M(area_visitor, MultiVisitor, GG_ShapeVisitor);

    printf("%s shape - area = %f\n", self->prefix, GG_Shape_GetArea(shape));
}

GG_IMPLEMENT_INTERFACE(AreaVisitor, GG_ShapeVisitor) {
    AreaVisitor_Visit
};

static void
ToStringVisitor_Visit(GG_ShapeVisitor* _self, const GG_Shape* shape)
{
    MultiVisitor* self = GG_SELF_M(to_string_visitor, MultiVisitor, GG_ShapeVisitor);

    char string[256];
    GG_Shape_ToString(shape, string, sizeof(string));

    printf("%s shape - to_string = %s\n", self->prefix, string);
}

GG_IMPLEMENT_INTERFACE(ToStringVisitor, GG_ShapeVisitor) {
    ToStringVisitor_Visit
};

static void
Example3()
{
    printf("* Example 3\n");

    GG_Rectangle* rectangle = NULL;
    GG_Result result = GG_Rectangle_Create(3, 4, &rectangle);
    GG_ASSERT(GG_SUCCEEDED(result));

    GG_Circle* circle = NULL;
    result = GG_Circle_Create(5, &circle);
    GG_ASSERT(GG_SUCCEEDED(result));

    // init the visitor owner
    MultiVisitor visitor;
    GG_SET_INTERFACE(&visitor.area_visitor, AreaVisitor, GG_ShapeVisitor);
    GG_SET_INTERFACE(&visitor.to_string_visitor, ToStringVisitor, GG_ShapeVisitor);
    visitor.prefix = "====";

    // visit the shapes with the first visitor, then the second visitor
    const GG_Shape* shapes[2] = {GG_CAST(rectangle, GG_Shape), GG_CAST(circle, GG_Shape)};
    VisitShapes(shapes, 2, GG_CAST(&visitor.area_visitor, GG_ShapeVisitor));
    VisitShapes(shapes, 2, GG_CAST(&visitor.to_string_visitor, GG_ShapeVisitor));

    // cleanup
    GG_Shape_Destroy(GG_CAST(rectangle, GG_Shape));
    GG_Shape_Destroy(GG_CAST(circle, GG_Shape));
}

//---------------------------------------------------------------------------------------
// 4. Using static interfaces
GG_DECLARE_INTERFACE(Example4Object1) {
    int (*Method1)(Example4Object1* self, int x);
    int (*Method2)(Example4Object1* self, const char* y);
};

static int Example4Object1_Method1(Example4Object1* self, int x) {
    return GG_INTERFACE(self)->Method1(self, x);
}

static int Example4Object1_Method2(Example4Object1* self, const char* y) {
    return GG_INTERFACE(self)->Method2(self, y);
}

GG_DECLARE_INTERFACE(Example4Object2) {
    void (*Method1)(Example4Object2* self);
};

static void Example4Object2_Method1(Example4Object2* self) {
    GG_INTERFACE(self)->Method1(self);
}

typedef struct {
    GG_IMPLEMENTS(Example4Object1);
    GG_IMPLEMENTS(Example4Object2);

    int         field1;
    const char* field2;
} MyFoo;

static int MyFoo_Example4Object1_Method1(Example4Object1* _self, int x) {
    MyFoo* self = GG_SELF(MyFoo, Example4Object1);
    printf("MyFoo_Example4Object1_Method1, field1=%d, field2=%s, x=%d\n", self->field1, self->field2, x);
    return 0;
}

static int MyFoo_Example4Object1_Method2(Example4Object1* _self, const char* y) {
    MyFoo* self = GG_SELF(MyFoo, Example4Object1);
    printf("MyFoo_Example4Object1_Method2, field1=%d, field2=%s, y=%s\n", self->field1, self->field2, y);
    return 0;
}

static void MyFoo_Example4Object2_Method1(Example4Object2* _self) {
    MyFoo* self = GG_SELF(MyFoo, Example4Object2);
    printf("MyFoo_Example4Object2_Method1, field1=%d, field2=%s\n", self->field1, self->field2);
}

GG_IMPLEMENT_INTERFACE(MyFoo, Example4Object1) {
    .Method1 = MyFoo_Example4Object1_Method1,
    .Method2 = MyFoo_Example4Object1_Method2
};

GG_IMPLEMENT_INTERFACE(MyFoo, Example4Object2) {
    .Method1 = MyFoo_Example4Object2_Method1
};

static MyFoo foo1 = {
    GG_INTERFACE_INITIALIZER(MyFoo, Example4Object1),
    GG_INTERFACE_INITIALIZER(MyFoo, Example4Object2),
    .field1 = 7,
    .field2 = "hello"
};

static MyFoo foo2 = {
    GG_INTERFACE_INLINE(Example4Object1, {
        .Method1 = MyFoo_Example4Object1_Method1,
        .Method2 = MyFoo_Example4Object1_Method2
    }),
    GG_INTERFACE_INLINE(Example4Object2, {
        .Method1 = MyFoo_Example4Object2_Method1
    }),
    .field1 = 8,
    .field2 = "bye bye"
};

static void
Example4(void) {
    Example4Object1_Method1(GG_CAST(&foo1, Example4Object1), 1234);
    Example4Object1_Method2(GG_CAST(&foo1, Example4Object1), "abcd");
    Example4Object2_Method1(GG_CAST(&foo1, Example4Object2));

    Example4Object1_Method1(GG_CAST(&foo2, Example4Object1), 5678);
    Example4Object1_Method2(GG_CAST(&foo2, Example4Object1), "efgh");
    Example4Object2_Method1(GG_CAST(&foo2, Example4Object2));
}

//---------------------------------------------------------------------------------------
// 5. Example where the GG_IMPLEMENT_INTERFACE is used inside a function, which can
// be done when there's no need to reference a class vtable outside of a single function
typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    int state;
} MyTimer;

static void
OnMyTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    MyTimer* self = GG_SELF(MyTimer, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);

    printf("OnMyTimerFired, time_elapsed = %u, state = %d\n", (int)time_elapsed, self->state);
}

static void
Example5(void) {

    MyTimer my_timer = {
        .state = 4567
    };
    GG_IMPLEMENT_INTERFACE(MyTimer, GG_TimerListener) {
        .OnTimerFired = OnMyTimerFired
    };
    GG_SET_INTERFACE(&my_timer, MyTimer, GG_TimerListener);

    GG_TimerListener_OnTimerFired(GG_CAST(&my_timer, GG_TimerListener), NULL, 1234);
}

//---------------------------------------------------------------------------------------
// 6. Static objects with no need for a type-defined struct and no 'self' (i.e no state)
// with an inline vtable
static void
OnMyTimerFired2(GG_TimerListener* self, GG_Timer* timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(timer);

    printf("OnMyTimerFired2, time_elapsed = %u\n", (int)time_elapsed);
}

static GG_TimerListener* my_timer =
    &GG_INTERFACE_INLINE_UNNAMED(GG_TimerListener, {
        .OnTimerFired = OnMyTimerFired2
    });

static void
Example6(void) {
    GG_TimerListener_OnTimerFired(my_timer, NULL, 1234);
}

//---------------------------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("Hello Golden Gate\n");

    Example1();
    Example2();
    Example3();
    Example4();
    Example5();
    Example6();

    return 0;
}
