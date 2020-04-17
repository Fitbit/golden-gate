/**
 * @file
 * @brief General purpose lists
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
#include <stdint.h>
#include <stddef.h>

//! @addtogroup Lists
//! General purpose lists.
//! @{

/*----------------------------------------------------------------------
|   Simple macro-based linked list, similar to BSD's queue.h macros
|
|   The implementation uses a doubly-linked circular list.
+---------------------------------------------------------------------*/

//!
//! Generic node entry to use as a member in a user-defined struct.
//!
typedef struct GG_LinkedListNode GG_LinkedListNode;
struct GG_LinkedListNode {
    GG_LinkedListNode* next; ///< Next node in the list
    GG_LinkedListNode* prev; ///< Previous node in the list
};

//!
//! Generic linked list of nodes.
//!
typedef GG_LinkedListNode GG_LinkedList;

#define GG_LINKED_LIST_NODE_NEXT(node) ((node)->next)              ///< Next node in the list
#define GG_LINKED_LIST_NODE_PREV(node) ((node)->prev)              ///< Previous node in the list
#define GG_LINKED_LIST_HEAD(list) (GG_LINKED_LIST_NODE_NEXT(list)) ///< Head node of the list
#define GG_LINKED_LIST_TAIL(list) (GG_LINKED_LIST_NODE_PREV(list)) ///< Tail node of the list

//!
//! Initialize a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_INIT(list)           \
do {                                        \
   GG_LINKED_LIST_NODE_NEXT(list) = (list); \
   GG_LINKED_LIST_NODE_PREV(list) = (list); \
} while (0)

//!
//! Initializer for a list node
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_INITIALIZER { NULL, NULL }

//!
//! Initializer for a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_INITIALIZER(list) { &(list), &(list) }

//!
//! Get a pointer to the item in which a node entry is embedded
//! @hideinitializer
//!
#define GG_LINKED_LIST_ITEM(node, type, field) \
  ((type *)((uintptr_t)(node) - offsetof(type, field)))

//!
//! Iterate over all the elements in a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_FOREACH(var, list)                    \
for (GG_LinkedListNode* var  = GG_LINKED_LIST_HEAD(list);    \
                        var != (list);                       \
                        var  = GG_LINKED_LIST_NODE_NEXT(var))

//!
//! Iterate over all the elements in a list
//! With this version of the iterator, it is ok to remove the current element from the list
//! @hideinitializer
//!
#define GG_LINKED_LIST_FOREACH_SAFE(var, list)                                         \
for (GG_LinkedListNode* var  = GG_LINKED_LIST_HEAD(list), *__##var = NULL;              \
                        var != (list) && (__##var = GG_LINKED_LIST_NODE_NEXT(var), 1); \
                        var  = __##var)

//!
//! Test if a list is empty
//! @hideinitializer
//!
#define GG_LINKED_LIST_IS_EMPTY(list) ((list) == GG_LINKED_LIST_HEAD(list))

//!
//! Initialize a list node
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_INIT(node)    \
do {                                      \
   GG_LINKED_LIST_NODE_NEXT(node) = NULL; \
   GG_LINKED_LIST_NODE_PREV(node) = NULL; \
} while (0)

//!
//! Test if a list node is unlinked
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_IS_UNLINKED(node) \
(GG_LINKED_LIST_NODE_NEXT(node) == NULL && GG_LINKED_LIST_NODE_PREV(node) == NULL)

//!
//! Concatenate two lists
//! @hideinitializer
//!
#define GG_LINKED_LIST_CONCAT(list, other)                                             \
do {                                                                                   \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_TAIL(list))  = GG_LINKED_LIST_HEAD(other); \
    GG_LINKED_LIST_NODE_PREV(GG_LINKED_LIST_HEAD(other)) = GG_LINKED_LIST_TAIL(list);  \
    GG_LINKED_LIST_TAIL(list) = GG_LINKED_LIST_TAIL(other);                            \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_TAIL(list) = (list);                       \
} while (0)

//!
//! Split a list at a node and assign the split portion to a new list
//! @hideinitializer
//!
#define GG_LINKED_LIST_SPLIT(list, split_node, dest)                   \
do {                                                                   \
    GG_LINKED_LIST_NODE_PREV(dest) = GG_LINKED_LIST_TAIL(list);        \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_NODE_PREV(dest)) = (dest); \
    GG_LINKED_LIST_HEAD(dest) = (split_node);                          \
    GG_LINKED_LIST_TAIL(list) = GG_LINKED_LIST_NODE_PREV(split_node);  \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_TAIL(list)) = (list);      \
    GG_LINKED_LIST_NODE_PREV(split_node) = (dest);                     \
} while (0)

//!
//! Move one list to another
//! @hideinitializer
//!
#define GG_LINKED_LIST_MOVE(list, dest)                              \
do {                                                                 \
    if (GG_LINKED_LIST_IS_EMPTY(list)) {                             \
        GG_LINKED_LIST_INIT(dest);                                   \
    } else {                                                         \
        GG_LinkedListNode* head = GG_LINKED_LIST_HEAD(list);         \
        GG_LINKED_LIST_SPLIT(list, head, dest);                      \
    }                                                                \
} while (0)

//!
//! Prepend a node to a list (insert the node at the head of the list)
//! @hideinitializer
//!
#define GG_LINKED_LIST_PREPEND(list, node)                        \
do {                                                              \
    GG_LINKED_LIST_NODE_NEXT(node) = GG_LINKED_LIST_HEAD(list);   \
    GG_LINKED_LIST_NODE_PREV(node) = (list);                      \
    GG_LINKED_LIST_NODE_PREV(GG_LINKED_LIST_HEAD(list)) = (node); \
    GG_LINKED_LIST_HEAD(list) = (node);                           \
} while (0)

//!
//! Append a node to a list (insert the node at the tail of the list)
//! @hideinitializer
//!
#define GG_LINKED_LIST_APPEND(list, node)                              \
do {                                                                   \
    GG_LINKED_LIST_NODE_NEXT(node) = (list);                           \
    GG_LINKED_LIST_NODE_PREV(node) = GG_LINKED_LIST_TAIL(list);        \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_NODE_PREV(node)) = (node); \
    GG_LINKED_LIST_TAIL(list) = (node);                                \
} while (0)

//!
//! Get and remove the head of the list.
//! The `node` argument must be a variable of type GG_LinkedListNode*.
//! It will be set to the head of the list if non empty, or NULL if empty.
//! @hideinitializer
//!
#define GG_LINKED_LIST_POP_HEAD(node, list)    \
do {                                           \
    if (GG_LINKED_LIST_IS_EMPTY(list)) {       \
        (node) = NULL;                         \
    } else {                                   \
        (node) = GG_LINKED_LIST_HEAD(list);    \
        GG_LINKED_LIST_NODE_REMOVE(node);      \
    }                                          \
} while (0)

//!
//! Get and remove the tail of the list.
//! The `node` argument must be a variable of type GG_LinkedListNode*.
//! It will be set to the tail of the list if non empty, or NULL if empty.
//! @hideinitializer
//!
#define GG_LINKED_LIST_POP_TAIL(node, list)    \
do {                                           \
    if (GG_LINKED_LIST_IS_EMPTY(list)) {       \
        (node) = NULL;                         \
    } else {                                   \
        (node) = GG_LINKED_LIST_TAIL(list);    \
        GG_LINKED_LIST_NODE_REMOVE(node);      \
    }                                          \
} while (0)

//!
//! Insert a node after another node in a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_INSERT_AFTER(after_node, node)                   \
do {                                                                         \
    GG_LINKED_LIST_NODE_NEXT(node) = GG_LINKED_LIST_NODE_NEXT(after_node);   \
    GG_LINKED_LIST_NODE_PREV(node) = (after_node);                           \
    GG_LINKED_LIST_NODE_PREV(GG_LINKED_LIST_NODE_NEXT(after_node)) = (node); \
    GG_LINKED_LIST_NODE_NEXT(after_node) = (node);                           \
} while (0)

//!
//! Insert a node before another node in a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_INSERT_BEFORE(before_node, node)                  \
do {                                                                          \
    GG_LINKED_LIST_NODE_NEXT(node) = (before_node);                           \
    GG_LINKED_LIST_NODE_PREV(node) = GG_LINKED_LIST_NODE_PREV(before_node);   \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_NODE_PREV(before_node)) = (node); \
    GG_LINKED_LIST_NODE_PREV(before_node) = (node);                           \
} while (0)

//!
//! Remove a node from a list
//! @hideinitializer
//!
#define GG_LINKED_LIST_NODE_REMOVE(node)                                                       \
do {                                                                                           \
    GG_LINKED_LIST_NODE_NEXT(GG_LINKED_LIST_NODE_PREV(node)) = GG_LINKED_LIST_NODE_NEXT(node); \
    GG_LINKED_LIST_NODE_PREV(GG_LINKED_LIST_NODE_NEXT(node)) = GG_LINKED_LIST_NODE_PREV(node); \
    GG_LINKED_LIST_NODE_INIT(node);                                                            \
} while (0)

//! @}
