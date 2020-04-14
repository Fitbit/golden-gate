/**
*
* @file: fb_smo_utils.c
*
* @copyright
* Copyright 2016 by Fitbit, Inc., all rights reserved.
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

#include "fb_smo_utils.h"

/*----------------------------------------------------------------------
|   Fb_SmoSimpleBlockAllocator_AllocateMemory
+---------------------------------------------------------------------*/
static void*
Fb_SmoSimpleBlockAllocator_AllocateMemory(Fb_SmoAllocator* _self, size_t size)
{
    Fb_SmoSimpleBlockAllocator* self = (Fb_SmoSimpleBlockAllocator*)_self;
    
    /* if this is the first block allocated, remember the block size */
    if (self->block_size == 0) {
        self->block_size = (unsigned int)size;
    } else {
        /* check the requested size */
        if (size != (size_t)self->block_size) {
            /* we can only allocate one block at a time */
            return NULL;
        }
    }
    
    /* check that we have enough memory */
    if (self->block_size * (self->blocks_used+1) > self->size) {
        /* out of memory */
        return NULL;
    }
    
    /* return the next available block */
    return self->blocks + (self->block_size * self->blocks_used++);
}

/*----------------------------------------------------------------------
|   Fb_SmoSimpleBlockAllocator_FreeMemory
+---------------------------------------------------------------------*/
static void
Fb_SmoSimpleBlockAllocator_FreeMemory(Fb_SmoAllocator* _self, void* memory)
{
    Fb_SmoSimpleBlockAllocator* self = (Fb_SmoSimpleBlockAllocator*)_self;
    (void)memory; // unused
    
    if (self->blocks_used) {
        --self->blocks_used;
    }
}

/*----------------------------------------------------------------------
|   Fb_SmoSimpleBlockAllocator_Init
+---------------------------------------------------------------------*/
void
Fb_SmoSimpleBlockAllocator_Init(Fb_SmoSimpleBlockAllocator* self, uint8_t* blocks, unsigned int size)
{
    self->base.allocate_memory = Fb_SmoSimpleBlockAllocator_AllocateMemory;
    self->base.free_memory     = Fb_SmoSimpleBlockAllocator_FreeMemory;
    self->blocks      = blocks;
    self->size        = size;
    self->block_size  = 0;
    self->blocks_used = 0;
}

/*----------------------------------------------------------------------
|   Fb_SmoGrowOnlyAllocator_AllocateMemory
+---------------------------------------------------------------------*/
static void*
Fb_SmoGrowOnlyAllocator_AllocateMemory(Fb_SmoAllocator* _self, size_t size)
{
    Fb_SmoGrowOnlyAllocator* self = (Fb_SmoGrowOnlyAllocator*)_self;
    void*                    memory;
    
    /* check that we have enough memory */
    if (self->used + (unsigned int)size > self->size) {
        /* out of memory */
        return NULL;
    }
    
    memory = self->heap+self->used;
    self->used += (unsigned int)size;
    
    return memory;
}

/*----------------------------------------------------------------------
|   Fb_SmoGrowOnlyAllocator_FreeMemory
+---------------------------------------------------------------------*/
static void
Fb_SmoGrowOnlyAllocator_FreeMemory(Fb_SmoAllocator* self, void* memory)
{
    (void)self;
    (void)memory;
}

/*----------------------------------------------------------------------
|   Fb_SmoGrowOnlyAllocator_Init
+---------------------------------------------------------------------*/
void
Fb_SmoGrowOnlyAllocator_Init(Fb_SmoGrowOnlyAllocator* self, uint8_t* heap, unsigned int size)
{
    self->base.allocate_memory = Fb_SmoGrowOnlyAllocator_AllocateMemory;
    self->base.free_memory     = Fb_SmoGrowOnlyAllocator_FreeMemory;
    self->heap                 = heap;
    self->size                 = size;
    self->used                 = 0;
}
