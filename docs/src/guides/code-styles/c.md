C Coding Conventions
====================

For code that is single-platform (such as iOS, Android, Bison,
or other platform-specific code modules), the coding conventions for that
platform are used.
For XP (Cross Platform) code, which is C code designed to be both independent
of platform-specific code repositories and platform-specific APIs,
the C coding conventions are specific to Golden Gate.

General C Conventions
---------------------

For things that work the same in C and C++, the coding style should follow the
Google [C++ Style Guide](https://google.github.io/styleguide/cppguide.html).
However, since the Golden Gate XP code is in C, not C++, there are few other
elements that come into play.

Namespacing
-----------

Since C doesn't offer builtin support for namespacing, we will use a poor-man's
namespacing technique, by using prefixed for all names that require a namespace
(mostly to avoid name collisions).
The common prefix for preprocessor symbols, function names and other visible
names is `GG_`.

(Why be so strict about namespace prefixing? Try looking for the constant
`MAX_MSG_SIZE` in a large code base with multiple independent modules and
submodules...)

Examples:

``` C
#define GG_LINK_MTU 251

void GG_ComputeCrc(const uint8_t* data, unsigned int data_size);

typedef enum {
    GG_PACKET_TYPE_FOO,
    GG_PACKET_TYPE_BAR
} GG_PacketType;

```

Object-Oriented C
-----------------

### Simple Classes

For code modules that implement class-like types, with encapsulated data and methods, the recommended naming convention for structs and functions is like this:

`GG_<class_name>_<method_name>`

where `<method_name>`` should read like an action performed on the type.

Examples

``` C
typedef struct GG_Socket GG_Socket; // opaque type for GG_Socket class

GG_Result GG_Socket_SendDatagram(GG_Socket* self, const GG_Datagram* datagram);
```

### Virtual Methods

For classes that are designed to have a shared abstract class and multiple concrete implementations, the recommendation is to define an abstract object base which consists of a named structure whose first field is a pointer to a function table.
The pattern looks like this for an interface named GG_Foobar:

``` C
typedef struct GG_FoobarInterface GG_FoobarInterface;
typedef struct {
    const GG_FoobarInterface* iface;
} GG_Foobar;
struct GG_FoobarInterface {
    GG_Result SomeMethod(GG_Foobar* self, uint8_t param);
};
```

To simplify the boilerplate, use the macro GG_DECLARE_INTERFACE, like this:
(which is equivalent to the above)

``` C
GG_DECLARE_INTERFACE(GG_Foobar)
{
    GG_Result SomeMethod(GG_Foobar* self, uint8_t param);
};

// Typical implementation of the method

```

### Polymorphism

For classes that implement multiple interfaces, the pattern is to embed multiple
abstract base class structures in the implemented class structure, and use
`offsetof()` to get pointers back to the instance data.
For example, a class that implements interfaces GG_Foo and GG_Bar would be
declared like this:

``` C
typedef struct {
    GG_IMPLEMENTS(GG_Foo);
    GG_IMPLEMENTS(GG_Bar);

    unsigned int my_member_variable;
} MyFoobarImplementation;
```

supposing that the `GG_Foo` and `GG_Bar` interfaces are declared like this:

``` C
GG_DECLARE_INTERFACE(GG_Foo)
{
    GG_Result DoSomething1(GG_Foo* self, uint8_t param1);
    GG_Result DoSomething2(GG_Foo* self, const char* param2);
};

GG_DECLARE_INTERFACE(GG_Bar)
{
    GG_Result WorkOnSomething(GG_Foo* self, const char* param);
};

```

Then in the implementation for `MyFooBarImplementation` may look like:

``` C
static GG_Result
MyFoobarImplementation_DoSomething1(GG_Foo* _self, uint8_t param1)
{
    MyFoobarImplementation* self = GG_SELF(MyFoobarImplementation, GG_Foo);

    // ...
}

static GG_Result
MyFoobarImplementation_DoSomething2(GG_Foo* _self, const char* param2)
{
    MyFoobarImplementation* self = GG_SELF(MyFoobarImplementation, GG_Bar);

    // ...
}

static GG_Result
MyFoobarImplementation_WorkOnSomething(GG_Bar* _self, const char* name)
{
    MyFoobarImplementation* self = GG_SELF(MyFoobarImplementation, GG_Bar);

    // ...
}
```

Setting up the function table pointers can also be done easily using the
`GG_SET_INTERFACE` macro:

``` C
GG_IMPLEMENT_INTERFACE(GG_Foo, MyFoobarImplementation)
{
    MyFoobarImplementation_DoSomething1,
    MyFoobarImplementation_DoSomething2
};

GG_IMPLEMENT_INTERFACE(GG_Bar, MyFoobarImplementation)
{
    MyFoobarImplementation_WorkOnSomething
};

static void
MyFoobarImplementation_Init(MyFoobarImplementation* self)
{
    // init the class members
    ...
    //

    // setup the function tables
    GG_SET_INTERFACE(self, MyFoobarImplementation, GG_Foo);
    GG_SET_INTERFACE(self, MyFoobarImplementation, GG_Bar);
}
```

File Names
----------

While not strictly necessary, it is recommended that file names be prefixed
as well.
Some build systems often use simple basenames and search paths to locate source
and header files, so "naked" file names can quickly become a problem in diverse
code base where we don't control all the files.
For example, naming one of our files `udp.c` would be asking for trouble.
That file should instead be named `gg_udp.c`, thus preventing possible
collisions. The same goes for include files (even though include files can
sometimes be prefixed implicitly by putting them in a uniquely named directory,
and using that directory name explicitly when including).
So `gg_udp.h` rather than just `udp.h`
