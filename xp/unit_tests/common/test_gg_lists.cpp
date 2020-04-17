// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_LISTS)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

//----------------------------------------------------------------------
typedef struct {
    int value;
    GG_LinkedListNode node;
    int other_value;
} Test1Item;

TEST(GG_LISTS, Test_LinkedLists_1) {
    GG_LinkedList list1 = GG_LINKED_LIST_INITIALIZER(list1);

    CHECK(GG_LINKED_LIST_IS_EMPTY(&list1));

    Test1Item item1 = {
        8, GG_LINKED_LIST_NODE_INITIALIZER, 9
    };
    GG_LINKED_LIST_NODE_INIT(&item1.node);
    Test1Item item2 = {
        10, GG_LINKED_LIST_NODE_INITIALIZER, 11
    };
    GG_LINKED_LIST_NODE_INIT(&item2.node);
    Test1Item item3 = {
        12, GG_LINKED_LIST_NODE_INITIALIZER, 13
    };
    GG_LINKED_LIST_NODE_INIT(&item3.node);

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    CHECK(!GG_LINKED_LIST_IS_EMPTY(&list1));
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    Test1Item* i = GG_LINKED_LIST_ITEM(&item1.node, Test1Item, node);
    CHECK(i == &item1);

    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item2.node);
    GG_LinkedListNode* n = GG_LINKED_LIST_NODE_NEXT(&i->node);
    CHECK(n == &item2.node);

    int counter1 = 0;
    int counter2 = 0;
    GG_LINKED_LIST_FOREACH(p, &list1) {
        i = GG_LINKED_LIST_ITEM(p, Test1Item, node);
        counter1 += i->value;
        counter2 += i->other_value;
    }
    CHECK(counter1 == item1.value + item2.value);
    CHECK(counter2 == item1.other_value + item2.other_value);

    GG_LINKED_LIST_PREPEND(&list1, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    n = GG_LINKED_LIST_HEAD(&list1);
    n = GG_LINKED_LIST_NODE_NEXT(n);
    n = GG_LINKED_LIST_NODE_NEXT(n);
    CHECK(n == &item2.node);

    GG_LINKED_LIST_NODE_REMOVE(&item1.node);

    GG_LinkedList list2 = GG_LINKED_LIST_INITIALIZER(list2);
    const GG_LinkedList* const_list = (const GG_LinkedList*)&list2;
    CHECK(GG_LINKED_LIST_IS_EMPTY(const_list));
}

//----------------------------------------------------------------------
typedef struct {
    int value;
    GG_LinkedListNode node;
} Test2Item;

TEST(GG_LISTS, Test_LinkedLists_2) {
    GG_LinkedList list1;
    GG_LINKED_LIST_INIT(&list1);

    Test2Item item1 = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item2 = { 2, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item3 = { 3, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item4 = { 4, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item5 = { 5, GG_LINKED_LIST_NODE_INITIALIZER };

    GG_LinkedListNode* tail = GG_LINKED_LIST_TAIL(&list1);
    GG_LINKED_LIST_NODE_INSERT_AFTER(tail, &item1.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    GG_LINKED_LIST_NODE_INSERT_AFTER(&item1.node, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_NODE_INSERT_AFTER(&item2.node, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);

    GG_LINKED_LIST_NODE_REMOVE(&item1.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item2.node);
    GG_LINKED_LIST_NODE_REMOVE(&item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    GG_LINKED_LIST_NODE_REMOVE(&item3.node);
    CHECK(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LinkedListNode* head = GG_LINKED_LIST_TAIL(&list1);
    GG_LINKED_LIST_NODE_INSERT_BEFORE(head, &item1.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    GG_LINKED_LIST_NODE_INSERT_BEFORE(&item1.node, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item2.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item1.node) == &item2.node);
    GG_LINKED_LIST_NODE_INSERT_BEFORE(&item2.node, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item2.node);

    GG_LINKED_LIST_NODE_REMOVE(&item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item2.node);
    GG_LINKED_LIST_NODE_REMOVE(&item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    GG_LINKED_LIST_NODE_REMOVE(&item3.node);
    CHECK(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    GG_LINKED_LIST_INIT(&list1);

    GG_LINKED_LIST_PREPEND(&list1, &item1.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    GG_LINKED_LIST_PREPEND(&list1, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item2.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item1.node) == &item2.node);
    GG_LINKED_LIST_PREPEND(&list1, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item2.node);
    GG_LINKED_LIST_INIT(&list1);

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);
    GG_LINKED_LIST_NODE_INSERT_AFTER(&item1.node, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_INIT(&list1);

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);
    GG_LINKED_LIST_NODE_INSERT_BEFORE(&item3.node, &item2.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_INIT(&list1);

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    GG_LINKED_LIST_APPEND(&list1, &item4.node);
    GG_LINKED_LIST_APPEND(&list1, &item5.node);
    GG_LINKED_LIST_NODE_INSERT_AFTER(&item2.node, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item4.node) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item5.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item4.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_INIT(&list1);

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    GG_LINKED_LIST_APPEND(&list1, &item4.node);
    GG_LINKED_LIST_APPEND(&list1, &item5.node);
    GG_LINKED_LIST_NODE_INSERT_BEFORE(&item4.node, &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item1.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item4.node) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item5.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item4.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
    GG_LINKED_LIST_INIT(&list1);
}

TEST(GG_LISTS, Test_LinkedLists_3) {
    GG_LinkedList list1;
    GG_LINKED_LIST_INIT(&list1);

    Test2Item item1 = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item2 = { 2, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item3 = { 3, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item4 = { 4, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item5 = { 5, GG_LINKED_LIST_NODE_INITIALIZER };

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);
    GG_LINKED_LIST_APPEND(&list1, &item4.node);
    GG_LINKED_LIST_APPEND(&list1, &item5.node);

    GG_LinkedList list2 = GG_LINKED_LIST_NODE_INITIALIZER;
    GG_LINKED_LIST_SPLIT(&list1, GG_LINKED_LIST_HEAD(&list1), &list2);
    CHECK(GG_LINKED_LIST_IS_EMPTY(&list1));
    CHECK(GG_LINKED_LIST_HEAD(&list2) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item2.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item4.node) == &item5.node);
    CHECK(GG_LINKED_LIST_TAIL(&list2) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item5.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item4.node) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item3.node) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);

    GG_LINKED_LIST_SPLIT(&list2, &item3.node, &list1);
    CHECK(GG_LINKED_LIST_HEAD(&list1) == &item3.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item3.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item4.node) == &item5.node);
    CHECK(GG_LINKED_LIST_TAIL(&list1) == &item5.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item5.node) == &item4.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item4.node) == &item3.node);
    CHECK(GG_LINKED_LIST_HEAD(&list2) == &item1.node);
    CHECK(GG_LINKED_LIST_NODE_NEXT(&item1.node) == &item2.node);
    CHECK(GG_LINKED_LIST_TAIL(&list2) == &item2.node);
    CHECK(GG_LINKED_LIST_NODE_PREV(&item2.node) == &item1.node);
}

TEST(GG_LISTS, Test_LinkedLists_4) {
    GG_LinkedList list1;
    GG_LINKED_LIST_INIT(&list1);
    GG_LinkedList list2;
    GG_LINKED_LIST_INIT(&list2);
    Test2Item item_z = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    GG_LINKED_LIST_APPEND(&list2, &item_z.node);

    GG_LinkedListNode* head = GG_LINKED_LIST_HEAD(&list2);
    CHECK_TRUE(GG_LINKED_LIST_IS_EMPTY(&list1));
    GG_LINKED_LIST_POP_HEAD(head, &list1);
    CHECK_TRUE(head == NULL);

    Test2Item item1 = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item2 = { 2, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item3 = { 3, GG_LINKED_LIST_NODE_INITIALIZER };

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);

    GG_LINKED_LIST_POP_HEAD(head, &list1);
    CHECK_FALSE(head == NULL);
    POINTERS_EQUAL(&item1, GG_LINKED_LIST_ITEM(head, Test2Item, node));
    CHECK_FALSE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_HEAD(head, &list1);
    CHECK_FALSE(head == NULL);
    POINTERS_EQUAL(&item2, GG_LINKED_LIST_ITEM(head, Test2Item, node));
    CHECK_FALSE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_HEAD(head, &list1);
    CHECK_FALSE(head == NULL);
    POINTERS_EQUAL(&item3, GG_LINKED_LIST_ITEM(head, Test2Item, node));
    CHECK_TRUE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_HEAD(head, &list1);
    CHECK_TRUE(head == NULL);
}

TEST(GG_LISTS, Test_LinkedLists_5) {
    GG_LinkedList list1;
    GG_LINKED_LIST_INIT(&list1);
    GG_LinkedList list2;
    GG_LINKED_LIST_INIT(&list2);
    Test2Item item_z = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    GG_LINKED_LIST_APPEND(&list2, &item_z.node);

    GG_LinkedListNode* tail = GG_LINKED_LIST_HEAD(&list2);
    CHECK_TRUE(GG_LINKED_LIST_IS_EMPTY(&list1));
    GG_LINKED_LIST_POP_HEAD(tail, &list1);
    CHECK_TRUE(tail == NULL);

    Test2Item item1 = { 1, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item2 = { 2, GG_LINKED_LIST_NODE_INITIALIZER };
    Test2Item item3 = { 3, GG_LINKED_LIST_NODE_INITIALIZER };

    GG_LINKED_LIST_APPEND(&list1, &item1.node);
    GG_LINKED_LIST_APPEND(&list1, &item2.node);
    GG_LINKED_LIST_APPEND(&list1, &item3.node);

    GG_LINKED_LIST_POP_TAIL(tail, &list1);
    CHECK_FALSE(tail == NULL);
    POINTERS_EQUAL(&item3, GG_LINKED_LIST_ITEM(tail, Test2Item, node));
    CHECK_FALSE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_TAIL(tail, &list1);
    CHECK_FALSE(tail == NULL);
    POINTERS_EQUAL(&item2, GG_LINKED_LIST_ITEM(tail, Test2Item, node));
    CHECK_FALSE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_TAIL(tail, &list1);
    CHECK_FALSE(tail == NULL);
    POINTERS_EQUAL(&item1, GG_LINKED_LIST_ITEM(tail, Test2Item, node));
    CHECK_TRUE(GG_LINKED_LIST_IS_EMPTY(&list1));

    GG_LINKED_LIST_POP_TAIL(tail, &list1);
    CHECK_TRUE(tail == NULL);
}
