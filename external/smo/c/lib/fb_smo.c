/**
*
* @file: fb_smo.c
*
* @copyright
* Copyright 2016-2020 Fitbit, Inc
* SPDX-License-Identifier: Apache-2.0
*
* @author Gilles Boccon-Gibod
*
* @date 2016-11-05
*
* @details
*
* Simple Message Object Model
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "fb_smo.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct Fb_Smo {
    char*        name;
    Fb_Smo*      parent;
    Fb_Smo*      first_child;
    Fb_Smo*      last_child;
    Fb_Smo*      next;
    Fb_Smo*      prev;
    Fb_SmoType   type;
    union {
        struct {
            uint8_t*     data;
            unsigned int size;
        }       bytes;
        char*   string;
        int64_t integer;
        double  floating_point;
    } value;
    Fb_SmoAllocator* allocator;
};

#if defined(FB_SMO_NULL_DEFAULT_ALLOCATOR)
/*----------------------------------------------------------------------
|   Fb_SmoNullAllocator_allocate_memory
+---------------------------------------------------------------------*/
static void*
Fb_SmoNullAllocator_allocate_memory(Fb_SmoAllocator* self, size_t size)
{
    (void)self;
    return NULL;
}

/*----------------------------------------------------------------------
|   Fb_SmoNullAllocator_free_memory
+---------------------------------------------------------------------*/
static void
Fb_SmoNullAllocator_free_memory(Fb_SmoAllocator* self, void* memory)
{
    (void)self;
    (void)memory;
}

/*----------------------------------------------------------------------
|   Fb_SmoDefaultAllocator
+---------------------------------------------------------------------*/
static const Fb_SmoAllocator
Fb_SmoNullAllocator = {
    Fb_SmoNullAllocator_allocate_memory,
    Fb_SmoNullAllocator_free_memory
};
Fb_SmoAllocator* const Fb_SmoDefaultAllocator = (Fb_SmoAllocator*)&Fb_SmoNullAllocator;

#else

/*----------------------------------------------------------------------
|   Fb_SmoStdlibAllocator_allocate_memory
+---------------------------------------------------------------------*/
static void*
Fb_SmoStdlibAllocator_allocate_memory(Fb_SmoAllocator* self, size_t size)
{
    (void)self;
    return malloc(size);
}

/*----------------------------------------------------------------------
|   Fb_SmoStdlibAllocator_free_memory
+---------------------------------------------------------------------*/
static void
Fb_SmoStdlibAllocator_free_memory(Fb_SmoAllocator* self, void* memory)
{
    (void)self;
    free(memory);
}

/*----------------------------------------------------------------------
|   Fb_SmoDefaultAllocator
+---------------------------------------------------------------------*/
static const Fb_SmoAllocator
Fb_SmoStdlibAllocator = {
    Fb_SmoStdlibAllocator_allocate_memory,
    Fb_SmoStdlibAllocator_free_memory
};
Fb_SmoAllocator* const Fb_SmoDefaultAllocator = (Fb_SmoAllocator*)&Fb_SmoStdlibAllocator;
#endif


/*----------------------------------------------------------------------
|   Fb_Smo_CreateBase
+---------------------------------------------------------------------*/
static Fb_Smo*
Fb_Smo_CreateBase(Fb_SmoAllocator* allocator, Fb_SmoType type, size_t extra_data_size)
{
    Fb_SmoAllocator* actual_allocator = (((allocator)?(allocator):Fb_SmoDefaultAllocator));
    Fb_Smo* smo = actual_allocator->allocate_memory(actual_allocator, sizeof(Fb_Smo)+extra_data_size);
    if (smo == NULL) return NULL;
    memset(smo, 0, sizeof(Fb_Smo));
    smo->type = type;
    smo->allocator = actual_allocator;

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_MovePointer
+---------------------------------------------------------------------*/
static void
Fb_Smo_MovePointer(Fb_Smo** new, Fb_Smo** old)
{
    *new = *old;
    *old = NULL;
}

/*----------------------------------------------------------------------
|   Fb_Smo_ReplaceObject
+---------------------------------------------------------------------*/
static Fb_Smo*
Fb_Smo_ReplaceObject(Fb_Smo* old, Fb_Smo* new)
{

    /*
     * Copies old object's name if needed
	 * and replace its place in the tree with new object
     * If returning new object old object is destroyed
     * Returns NULL if memory allocation fails for object name and new object is destroyed or
	 * if new object doesn't exist
     */

    if (new != NULL) {
        if (old->name != NULL) {
            new->name = new->allocator->allocate_memory(new->allocator, strlen(old->name) + 1);
            if (new->name != NULL) {
                strcpy(new->name, old->name);
            } else {
                // Destroy new object
                Fb_Smo_Destroy(new);
                new = NULL;
            }
        }

        // Fail if memory allocation for name failed
        if (new != NULL) {
            Fb_Smo* child;

            Fb_Smo_MovePointer(&(new->parent), &(old->parent));
            Fb_Smo_MovePointer(&(new->prev), &(old->prev));
            Fb_Smo_MovePointer(&(new->next), &(old->next));
            Fb_Smo_MovePointer(&(new->first_child), &(old->first_child));
            Fb_Smo_MovePointer(&(new->last_child), &(old->last_child));

            // Update parent
            if (new->parent){
                if (new->parent->first_child == old) {
                    new->parent->first_child = new;
                }
                if (new->parent->last_child == old) {
                    new->parent->last_child = new;
                }
            }

            // Update siblings
            if (new->next != NULL) {
                new->next->prev = new;
            }
            if (new->prev != NULL) {
                new->prev->next = new;
            }

            // Update children
            child = new->first_child;
            while (child != NULL) {
                child->parent = new;
                child = child->next;
            }

            // Destroy original object
            Fb_Smo_Destroy(old);
        }
    }

    return new;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateObject
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateObject(Fb_SmoAllocator* allocator)
{
    return Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_OBJECT, 0);
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateArray
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateArray(Fb_SmoAllocator* allocator)
{
    return Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_ARRAY, 0);
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateString
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateString(Fb_SmoAllocator* allocator, const char* value, unsigned int value_length)
{
    Fb_Smo* smo;

    /* compute the size of the value and allocate the base object */
    if (value_length == 0 && value) value_length = (unsigned int)strlen(value);
    smo = Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_STRING, value_length+1);
    if (smo == NULL) return NULL;

    /* make a copy of the string */
    smo->value.string = ((char*)smo)+sizeof(Fb_Smo);
    if (value && value_length) {
        memcpy(smo->value.string, value, value_length);
    }
    smo->value.string[value_length] = '\0';

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateBytes
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateBytes(Fb_SmoAllocator* allocator, const uint8_t* value, unsigned int value_size)
{
    /* allocate the base object */
    Fb_Smo* smo = Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_BYTES, value_size);
    if (smo == NULL) return NULL;

    /* make a copy of the bytes */
    smo->value.bytes.size = value_size;
    if (value_size) {
        smo->value.bytes.data = ((uint8_t*)smo)+sizeof(Fb_Smo);
        memcpy(smo->value.bytes.data, value, value_size);
    } else {
        /* 0-size byte array maps to NULL */
        smo->value.bytes.data = NULL;
    }

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateInteger
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateInteger(Fb_SmoAllocator* allocator, int64_t value)
{
    /* allocate the base object */
    Fb_Smo* smo = Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_INTEGER, 0);
    if (smo == NULL) return NULL;

    /* set the value */
    smo->value.integer = value;

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateFloat
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateFloat(Fb_SmoAllocator* allocator, double value)
{
    /* allocate the base object */
    Fb_Smo* smo = Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_FLOAT, 0);
    if (smo == NULL) return NULL;

    /* set the value */
    smo->value.floating_point = value;

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_CreateSymbol
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_CreateSymbol(Fb_SmoAllocator* allocator, Fb_SmoSymbol value)
{
    /* allocate the base object */
    Fb_Smo* smo = Fb_Smo_CreateBase(allocator, FB_SMO_TYPE_SYMBOL, 0);
    if (smo == NULL) return NULL;

    /* set the value */
    smo->value.integer = (int64_t)value;

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_Create
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_Create(Fb_SmoAllocator* allocator, const char* spec, ...)
{
    Fb_Smo*      root          = NULL;
    Fb_Smo*      context       = NULL;
    const char*  name          = NULL;
    unsigned int name_length   = 0;
    unsigned int syntax_error  = 0;
    char         c;
    va_list      args;

    va_start(args, spec);

    while ((c = *spec++)) {
        Fb_Smo* smo = NULL;

        if (context) {
            if (context->type == FB_SMO_TYPE_OBJECT && c != '}') {
                if (name == NULL) {
                    /* look for the name in the spec, ending with '=' */
                    const char* name_end = spec-1;
                    name = name_end;
                    while (*name_end && *name_end != '=') {
                        ++name_end;
                    }
                    if (*name_end != '=') {
                        syntax_error = 1;
                        break;
                    }
                    if (c == '=') {
                        /* get the name from the args */
                        name = va_arg(args, char*);
                    } else {
                        name_length = (unsigned int)(name_end-name);
                        spec = name_end+1;
                    }
                    continue;
                }
            }
        }

        switch (c) {
            case 'i':
                smo = Fb_Smo_CreateInteger(allocator, va_arg(args, int));
                break;

            case 'I':
                smo = Fb_Smo_CreateInteger(allocator, va_arg(args, int64_t));
                break;

            case 'f':
                smo = Fb_Smo_CreateFloat(allocator, va_arg(args, double));
                break;

            case 's':
                smo = Fb_Smo_CreateString(allocator, va_arg(args, char*), 0);
                break;

            case 'b':
                {
                    uint8_t*     bytes      = va_arg(args, uint8_t*);
                    unsigned int bytes_size = va_arg(args, unsigned int);
                    smo = Fb_Smo_CreateBytes(allocator, bytes, bytes_size);
                }
                break;

            case '#':
                smo = Fb_Smo_CreateSymbol(allocator, (Fb_SmoSymbol)va_arg(args, int));
                break;

            case 'N':
                smo = Fb_Smo_CreateSymbol(allocator, FB_SMO_SYMBOL_NULL);
                break;

            case 'T':
                smo = Fb_Smo_CreateSymbol(allocator, FB_SMO_SYMBOL_TRUE);
                break;

            case 'F':
                smo = Fb_Smo_CreateSymbol(allocator, FB_SMO_SYMBOL_FALSE);
                break;

            case 'U':
                smo = Fb_Smo_CreateSymbol(allocator, FB_SMO_SYMBOL_UNDEFINED);
                break;

            case '[':
                smo = Fb_Smo_CreateArray(allocator);
                break;

            case '{':
                smo = Fb_Smo_CreateObject(allocator);
                break;

            case ']':
            case '}':
                if (context && context->type == (c==']' ? FB_SMO_TYPE_ARRAY : FB_SMO_TYPE_OBJECT)) {
                    context = context->parent;
                } else {
                    syntax_error = 1;
                }
                break;

            default:
                break;
        }

        /* stop now if we couldn't create an object but were expected to */
        if (smo == NULL) {
            if (syntax_error) {
                break;
            } else {
                continue;
            }
        }

        /* store the new object in its container */
        if (context) {
            if (context->type == FB_SMO_TYPE_OBJECT || context->type == FB_SMO_TYPE_ARRAY) {
                Fb_Smo_AddChild(context, name, name_length, smo);
                name = NULL;
                name_length = 0;
            }
        } else {
            /* the first object created becomes the root */
            if (root == NULL) {
                root = smo;
            } else {
                /* the current context can't have children */
                if (smo) {
                    Fb_Smo_Destroy(smo);
                    smo = NULL;
                    syntax_error = 1;
                }
                break;
            }
        }

        /* update the context */
        if (smo->type == FB_SMO_TYPE_ARRAY || smo->type == FB_SMO_TYPE_OBJECT) {
            context = smo;
        }
    }

    va_end(args);

    /* check that we're at the top level without errors and cleanup if we're not */
    if (context || syntax_error) {
        if (root) {
            Fb_Smo_Destroy(root);
            root = NULL;
        }
    }

    /* done */
    return root;
}


/*----------------------------------------------------------------------
|   Fb_Smo_Destroy
|
|   Non-recursive implementation
+---------------------------------------------------------------------*/
int
Fb_Smo_Destroy(Fb_Smo* self)
{
    Fb_Smo* smo = self;

    /* detach from our family if we have one */
    if (smo->parent) {
        if (smo->prev) {
            smo->prev->next = smo->next;
        }
        if (smo->next) {
            smo->next->prev = smo->prev;
        }
        if (smo->parent->first_child == smo) {
            smo->parent->first_child = smo->next;
        }
        if (smo->parent->last_child == smo) {
            smo->parent->last_child = smo->prev;
        }
        smo->next   = NULL;
        smo->prev   = NULL;
        smo->parent = NULL;
    }

    /* visit the entire subtree and free everything */
    while (smo) {
        Fb_Smo* next;

        /* always process children first */
        if (smo->first_child) {
            Fb_Smo* first_child = smo->first_child;
            smo->first_child    = NULL;
            smo->last_child     = NULL;
            smo                 = first_child;
            continue;
        }

        /* remember the next sibling or parent for after we free this object */
        next = smo->next ? smo->next : smo->parent;

        /* free dynamically allocated name */
        if (smo->name) {
            smo->allocator->free_memory(smo->allocator, smo->name);
        }

        /* free the object */
        smo->allocator->free_memory(smo->allocator, smo);

        /* move on to the next child or up to the parent */
        smo = next;
    }

    return FB_SMO_SUCCESS;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetChildrentCount
+---------------------------------------------------------------------*/
unsigned int
Fb_Smo_GetChildrenCount(Fb_Smo* self)
{
    unsigned int count = 0;
    Fb_Smo* child = self->first_child;
    while (child) {
        ++count;
        child = child->next;
    }
    return count;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetFirstChild
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_GetFirstChild(Fb_Smo* self)
{
    return self->first_child;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetChildByName
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_GetChildByName(Fb_Smo* self, const char* name)
{
    Fb_Smo* child = self->first_child;
    while (child) {
        if (child->name && !strcmp(child->name, name)) {
            return child;
        }
        child = child->next;
    }

    return NULL;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetDescendantByPath
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_GetDescendantByPath(Fb_Smo* self, const char* path)
{
    const char* name = path;
    char c;
    Fb_Smo* smo;

    /* exit early if this object has no descendent */
    if (!self->first_child) return NULL;

    /* start from here */
    smo = self;

    /* walk the path */
    do {
        c = *path++;
        if (c == '.' || c == '[' || c == '\0') {
            unsigned int name_size = (unsigned int)(path-name-1);
            if (name_size == 0) {
                /* empty name */
                if (name != path-1) {
                    /* empty names are only allowed at the start of the path */
                    return NULL;
                } else {
                    continue;
                }
            }

            /* look for the requested child */
            smo = smo->first_child;
            if (name[0] == '[') {
                /* parse the index */
                unsigned int index = 0;
                ++name;
                while (*name >= '0' && *name <= '9') {
                    index = 10 * index + (unsigned char)(*name++ - '0');
                }
                if (*name != ']') return NULL; /* invalid syntax */

                /* get the child at the specified index */
                while (smo && index--) {
                    smo = smo->next;
                }
            } else {
                /* the name is a field name */
                while (smo) {
                    if (smo->name && !strncmp(smo->name, name, name_size)) {
                        break;
                    }
                    smo = smo->next;
                }
            }
            name = (c == '.' ? path : path-1);
        }
    } while (c && smo);

    return smo;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetNext
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_GetNext(Fb_Smo* self)
{
    return self->next;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetParent
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_GetParent(Fb_Smo* self)
{
    return self->parent;
}

/*----------------------------------------------------------------------
|   Fb_Smo_AddChild
+---------------------------------------------------------------------*/
int
Fb_Smo_AddChild(Fb_Smo* self, const char* name, unsigned int name_length, Fb_Smo* child)
{
    /* check that we can add a child to this object */
    if (self->type == FB_SMO_TYPE_ARRAY) {
        if (name != NULL) {
            /* array children don't have names */
            return FB_SMO_ERROR_INVALID_PARAMETERS;
        }
    } else if (self->type != FB_SMO_TYPE_OBJECT) {
        return FB_SMO_ERROR_INVALID_PARAMETERS;
    }

    /* check that the child isn't part of some tree already */
    if (child->parent || child->name) {
        return FB_SMO_ERROR_INVALID_PARAMETERS;
    }

    /* name the child */
    if (name) {
        if (name_length == 0) name_length = (unsigned int)strlen(name);
        child->name = child->allocator->allocate_memory(child->allocator, name_length+1);
        if (child->name == NULL) return FB_SMO_ERROR_OUT_OF_MEMORY;
        if (name && name_length) {
            memcpy(child->name, name, name_length);
        }
        child->name[name_length] = '\0';
    }

    /* setup the family relationships to make this child the last child */
    child->parent = self;
    child->next   = NULL;
    if (self->last_child) {
        child->prev            = self->last_child;
        self->last_child->next = child;
    } else {
        child->prev       = NULL;
        self->first_child = child;
    }
    self->last_child = child;

    return FB_SMO_SUCCESS;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetType
+---------------------------------------------------------------------*/
Fb_SmoType
Fb_Smo_GetType(Fb_Smo* self)
{
    return self->type;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetName
+---------------------------------------------------------------------*/
const char*
Fb_Smo_GetName(Fb_Smo* self)
{
    return self->name;
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetValueAsString
+---------------------------------------------------------------------*/
const char*
Fb_Smo_GetValueAsString(Fb_Smo* self)
{
    if (self->type == FB_SMO_TYPE_STRING) {
        return self->value.string;
    } else {
        return NULL;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetValueAsBytes
+---------------------------------------------------------------------*/
const uint8_t*
Fb_Smo_GetValueAsBytes(Fb_Smo* self, unsigned int* size)
{
    if (self->type == FB_SMO_TYPE_BYTES) {
        if (size) {
            *size = self->value.bytes.size;
        }
        return self->value.bytes.data;
    } else {
        if (size) {
            *size = 0;
        }
        return NULL;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetValueAsInteger
+---------------------------------------------------------------------*/
int64_t
Fb_Smo_GetValueAsInteger(Fb_Smo* self)
{
    if (self->type == FB_SMO_TYPE_INTEGER) {
        return self->value.integer;
    } else if (self->type == FB_SMO_TYPE_FLOAT) {
        return (int64_t)self->value.floating_point;
    } else {
        return 0;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetValueAsFloat
+---------------------------------------------------------------------*/
double
Fb_Smo_GetValueAsFloat(Fb_Smo* self)
{
    if (self->type == FB_SMO_TYPE_FLOAT) {
        return self->value.floating_point;
    } else if (self->type == FB_SMO_TYPE_INTEGER) {
        return (double)self->value.integer;
    } else {
        return 0.0;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_GetValueAsSymbol
+---------------------------------------------------------------------*/
Fb_SmoSymbol
Fb_Smo_GetValueAsSymbol(Fb_Smo* self)
{
    if (self->type == FB_SMO_TYPE_SYMBOL) {
        return (Fb_SmoSymbol)self->value.integer;
    } else {
        /* reasonable default */
        return FB_SMO_SYMBOL_NULL;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_SetValueAsString
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_SetValueAsString(Fb_Smo* self, const char* value, unsigned int value_length)
{

    if (self->type == FB_SMO_TYPE_STRING) {

        /* compute the size of the value if not given */
        if (value_length == 0 && value) value_length = (unsigned int)strlen(value);

        if (strlen(self->value.string) < value_length) {
            Fb_Smo* copy;

            /* Size is larger than currently so create new object and replace it in the tree */
            copy = Fb_Smo_CreateString(self->allocator, value, value_length);
            self = Fb_Smo_ReplaceObject(self, copy);
        }

        /* If memory allocation has failed when resizing or replacing object
         * don't do anything and return NULL
         */
        if (self != NULL) {
            self->value.string = ((char*)self)+sizeof(Fb_Smo);
            if (value && value_length) {
                memcpy(self->value.string, value, value_length);
            }
            self->value.string[value_length] = '\0';
        }
    }

    return self;
}

/*----------------------------------------------------------------------
|   Fb_Smo_SetValueAsBytes
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_SetValueAsBytes(Fb_Smo* self, const uint8_t* value, unsigned int value_size)
{
    if (self->type == FB_SMO_TYPE_BYTES) {

        if (self->value.bytes.size < value_size) {
            Fb_Smo* copy;

            /* Size is larger than currently so create new object and replace it in the tree */
            copy = Fb_Smo_CreateBytes(self->allocator, value, value_size);
            self = Fb_Smo_ReplaceObject(self, copy);
        }

        /* If memory allocation has failed when resizing or replacing object
         * don't do anything and return NULL
         */
        if (self != NULL) {
            /* make a copy of the bytes */
            self->value.bytes.size = value_size;
            if (value_size) {
                self->value.bytes.data = ((uint8_t*)self)+sizeof(Fb_Smo);
                memcpy(self->value.bytes.data, value, value_size);
            } else {
                /* 0-size byte array maps to NULL */
                self->value.bytes.data = NULL;
            }
        }
    }

    return self;
}

/*----------------------------------------------------------------------
|   Fb_Smo_SetValueAsInteger
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_SetValueAsInteger(Fb_Smo* self, int64_t value)
{
    if (self->type == FB_SMO_TYPE_INTEGER) {
        /* set the value */
        self->value.integer = value;
    }
    return self;
}

/*----------------------------------------------------------------------
|   Fb_Smo_SetValueAsFloat
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_SetValueAsFloat(Fb_Smo* self, double value)
{
    if (self->type == FB_SMO_TYPE_FLOAT) {
        /* set the value */
        self->value.floating_point = value;
    }
    return self;
}

/*----------------------------------------------------------------------
|   Fb_Smo_SetValueAsSymbol
+---------------------------------------------------------------------*/
Fb_Smo*
Fb_Smo_SetValueAsSymbol(Fb_Smo* self, Fb_SmoSymbol value)
{
    if (self->type == FB_SMO_TYPE_SYMBOL) {
        /* set the value */
        self->value.integer = (int64_t)value;
    }
    return self;
}
