/**
 * @file
 * @brief General purpose type definitions and interface macros
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>

//! @addtogroup Types
//! Common shared types and macros
//! @{

/*----------------------------------------------------------------------
|   common types
+---------------------------------------------------------------------*/
typedef uint64_t GG_Position;     ///< Position in a file or stream
typedef uint64_t GG_Timestamp;    ///< Timestamp in nanoseconds since an arbitrary origin
typedef uint64_t GG_TimeInterval; ///< Time interval in nanoseconds
typedef uint64_t GG_Timeout;      ///< Timeout value in nanoseconds

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#if !defined(NULL)
#define NULL ((void*)0) ///< NULL pointer
#endif

#define GG_TIMEOUT_INFINITE ((GG_TimeInterval)-1) ///< Infinite timeout (wait forever)

/**
 * Macro that constructs a 32-bit integer from 4 characters (4CC = Four Character Code)
 */
#define GG_4CC(a, b, c, d) ( ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | ((uint32_t)d) )

/** @} */

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
typedef void (*GG_GenericInterfaceTrapMethod)(const void*);
extern GG_GenericInterfaceTrapMethod const GG_GenericInterfaceTrapVTable[];

//----------------------------------------------------------------------
//  Interface macros
//
//! @addtogroup Interfaces
//! Abstract Interfaces
//! @{
//----------------------------------------------------------------------

/**
 * Name of the field used to hold the interface base class in an instance
 */
#define GG_INTERFACE_BASE_FIELD(_iface) _iface##_Base

/**
 * Macro used to obtain a pointer to the containing object from a pointer to an interface implemented by
 * that object.
 * The use of this macro assumes that the pointer to the interface is a variable named `_self`, typically
 * the first parameter in a method implementation function.
 *
 * Example:
 * ```
 * typedef struct {
 *     GG_IMPLEMENTS(GG_Foobar);
 *     // other fields...
 * } MyFooar;
 *
 * void MyFoobar_SomeMethod(GG_Foobar* _self) {
 *     MyFoobar* self = GG_SELF(MyFoobar, GG_Foobar);
 *     // do something that accesses the fields of self
 * }
 * ```
 */
#define GG_SELF(_self_type, _iface) GG_SELF_O(_self, _self_type, _iface)
#define GG_SELF_O(_object, _self_type, _iface) \
    ( (_self_type *)( ((uintptr_t)(_object)) - offsetof(_self_type, GG_INTERFACE_BASE_FIELD(_iface))) )

/**
 * Macro similar to `GG_SELF`, but for when the object declares that it implements an interface as part of
 * an inner object/struct field.
 *
 * Example:
 * ```
 * typedef struct {
 *     struct {
 *         GG_IMPLEMENTS(GG_Foobar);
 *         // other fields...
 *     } inner;
 *     // other fields...
 * } MyFooar;
 *
 * void MyFoobar_SomeMethod(GG_Foobar* _self) {
 *     MyFoobar* self = GG_SELF_M(inner, MyFoobar, GG_Foobar);
 *     // do something that accesses the fields of self
 * }
 * ```
 */
#define GG_SELF_M(_member, _self_type, _iface) GG_SELF_M_O(_self, _member, _self_type, _iface)
#define GG_SELF_M_O(_object, _member, _self_type, _iface) \
    ( (_self_type *)( ((uintptr_t)(_object)) - offsetof(_self_type, _member.GG_INTERFACE_BASE_FIELD(_iface))) )

/**
 * Macro used to obtain a pointer to one of the interfaces implemented by that object.
 *
 * Example:
 * ```
 * GG_Foobar* foobar = GG_CAST(my_object, GG_Foobar);
 * ```
 */
#define GG_CAST(_object, _iface) &(_object)->GG_INTERFACE_BASE_FIELD(_iface)

/**
 * Macro used to declare an abstract interface.
 *
 * The parameter is the name of an interface to define.
 * For a name `Foo`, this will declare an object type `Foo` that can be used
 * to type pointers to objects that implement the `Foo` interface, as well
 * as a virtual function table `FooInterface` that is a structure containing
 * fields that are function pointers, one for each method of the interface.
 *
 * This macro must be followed directly (without `;`) by an opening `{`, then methods expressed
 * as function pointers, and finally a closing `};`
 *
 * The first parameter of each method declaration *must* be a pointer to the object type.
 *
 * Example:
 * ```
 * GG_DECLARE_INTERFACE(GG_Foobar) {
 *     GG_Result (*Method1)(GG_Foobar* self, int param);
 *     GG_Result (*Method2)(GG_Foobar* self, const char* param);
 * }
 * ```
 */
#define GG_DECLARE_INTERFACE(_iface)                 \
typedef struct _iface##Interface _iface##Interface;  \
typedef struct {                                     \
    const _iface##Interface* vtable;                 \
} _iface;                                            \
struct _iface##Interface

/**
 * Macro used to define the virtual function table for a class
 * that implements an interface.
 *
 * The first parameter is the name of the class, and the second parameter is the name
 * of an interface implemented by the class.
 *
 * This macro must be followed directly (without `;`) by an opening `{`, then the names
 * of the function that implement each of the methods of the interface, separated by `,`,
 * then a closing `};`
 *
 * Example:
 * ```
 * GG_IMPLEMENT_INTERFACE(MyClass, GG_Foobar) {
 *     .Method1 = MyClass_Method1,
 *     .Method2 = MyClass_Method2
 * };
 * ```
 */
#define GG_IMPLEMENT_INTERFACE(_class, _iface) static const _iface##Interface _class##_##_iface##Interface =

/**
 * Macro used to setup a virtual function table pointer for an instance of a class that implements
 * an interface.
 *
 * The first parameter is the object instance.
 * The second parameter is the name of the class of the instance.
 * The third parameter is the name of the interface for which to set the virtual function
 * table pointer.
 *
 * Example:
 * ```
 * GG_SET_INTERFACE(my_object, MyObjectType, GG_Foobar);
 * ```
 */
#define GG_SET_INTERFACE(_object, _class, _iface) do {                                 \
    (_object)->GG_INTERFACE_BASE_FIELD(_iface).vtable = &_class##_##_iface##Interface; \
} while (0)

/**
 * Macro used to set an interface trap. A trap is a special implementation of an interface
 * that can be used to trap calls made through that interface when it shouldn't have. This
 * is typically used in object destructors, so that if a destroyed object is used after its
 * memory has been free'd, the trap will be called (unless of course that memory has been
 * overwritten with something else).
 *
 * The first parameter is the object instance.
 * The second parameter is the name of the interface for which to set a trap.
 *
 * Example:
 * ```
 * GG_SET_INTERFACE_TRAP(my_object, GG_Foobar);
 * ```
 */
#define GG_SET_INTERFACE_TRAP(_object, _iface) do {                                                 \
    (_object)->GG_INTERFACE_BASE_FIELD(_iface).vtable = (const void*)GG_GenericInterfaceTrapVTable; \
} while (0)

/**
 * Macro used to initialize an interface pointer as part of an object's initialization declaration.
 *
 * Example:
 * ```
 * MyFoobar f = {
 *     GG_INTERFACE_INITIALIZER(MyFoobar, GG_Foobar),
 *     .my_field = 123
 *     // ...
 * };
 * ```
 */
#define GG_INTERFACE_INITIALIZER(_class, _iface) \
    .GG_INTERFACE_BASE_FIELD(_iface) = { .vtable = &_class##_##_iface##Interface }

/**
 * Macro used to initialize an interface pointer as part of an object's initialization, where the
 * function table for the interface is anynoymous, supplied locally, inline, rather than using a named function
 * table (which would be defined by using the `GG_IMPLEMENT_INTERFACE` macro).
 *
 * WARNING: users of this macro must be careful when using this with non-static initializers, because in that case
 * the locally initialized vtable's lifetime may not match that of the object that refers to it.
 *
 * Example:
 * ```
 * MyFoobar f = {
 *    GG_INTERFACE_INLINE(GG_Foobar, {
 *        .Method1 = MyFoobar_Method1,
 *        .Method2 = MyFoobar_Method2
 *    }),
 *    .my_field = 123
 *    // ...
 * };
 * ```
 */
#define GG_INTERFACE_INLINE(_iface, ...) \
    .GG_INTERFACE_BASE_FIELD(_iface) = { .vtable = &(_iface##Interface) __VA_ARGS__ }

/**
 * Macro similar to `GG_INTERFACE_INLINE`, but used when the function table pointer is not
 * necessarily stored in a named struct field that was declared with `GG_IMPLEMENTS`.
 *
 * The first parameter is the name of the interface. The second parameter is a function table,
 * expressed as a list of functions enclosed by '{' and '}'.
 *
 * This is normally used with static objects, as the anonymous function table is declared
 * inline. Also, because the function table is not stored in a named field in a type-defined struct,
 * the method implementations cannot use the GG_SELF macro to access any object state. So this
 * is typically reserved for objects with no state, or with purely static state.
 *
 * Example:
 * ```
 * static GG_Foobar*
 * my_foobar = &GG_INTERFACE_INLINE_UNNAMED(GG_Foobar, {
 *     .Method1 = MyClass_Method1,
 *     .Method2 = MyClass_Method2
 * });
 * ```
 */
#define GG_INTERFACE_INLINE_UNNAMED(_iface, ...) \
    (_iface) { .vtable = &(_iface##Interface) __VA_ARGS__ }

/**
 * Macro used in a class definition (`struct` keyword, since this is C) to declare that the
 * class implements a given interface. This macro must be used once per interface implemented
 * by the class.
 * By convention, this macro should appear at the top of the class declaration, before member fields.
 *
 * Example:
 * ```
 * typedef struct MyClass {
 *     GG_IMPLEMENTS(GG_Foo);
 *     GG_IMPLEMENTS(GG_Bar);
 *     int my_field_1;
 *     int my_field_2;
 * };
 * ```
 */
#define GG_IMPLEMENTS(_iface) _iface GG_INTERFACE_BASE_FIELD(_iface)

/**
 * Macro that evaluates to the virtual function table for an interface pointer.
 *
 * This is typically used when implenting `thunk` functions for invoking methods on objects.
 *
 * Example:
 * ```
 * GG_Result
 * GG_Foobar_Method1(const GG_Foobar* self, int param) {
 *     return GG_INTERFACE(self)->Method1(self, param);
 * }
 * ```
 */
#define GG_INTERFACE(_iface) ((_iface)->vtable)

//! @}
