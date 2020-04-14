/**
*
* @file: fb_smo_jerryscript.c
*
* @copyright
* Copyright 2017 by Fitbit, Inc., all rights reserved.
*
* @author Gilles Boccon-Gibod
*
* @date 2017-03-20
*
* @details
*
* SMO JerryScript bindings
*
*/

#if !defined(_FB_SMO_JERRYSCRIPT_H_)
#define _FB_SMO_JERRYSCRIPT_H_

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include "fb_smo.h"
#include <jerryscript.h>

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * This interface is used to feed the deserializer without having to load the whole
 * serialized data buffer in memory.
 */
typedef struct Fb_SmoJerryDataSource Fb_SmoJerryDataSource;
struct Fb_SmoJerryDataSource {
    /**
     * Try to load more data in memory and return a pointer to the (possibly re-allocated)
     * buffer and the number of bytes available in the buffer.
     *
     * @param[in] self             Source object on which the method is called
     * @param[out] buffer          Pointer to the buffer in which the current data resides
     * @param[out] bytes_available Number of bytes available in the buffer
     *
     * @return The numnber of bytes added to the buffer.
     */
    unsigned int (*get_more)(Fb_SmoJerryDataSource* self, const uint8_t** buffer, unsigned int* bytes_available);
    
    /**
     * Notify the source that one or more bytes have been used and can therefore
     * be pruged/freed from any internal state/storage the source may have.
     *
     * @param[in] self       Source object on which the method is called
     * @param[in] bytes_used Number of bytes that have been used by the caller
     *
     * @return FB_SMO_SUCCESS if the call succeeds, or FB_SMO_ERROR_XXX if it fails
     */
    int (*advance)(Fb_SmoJerryDataSource* self, unsigned int bytes_used);
};

/*----------------------------------------------------------------------
|   prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * Deserialize a CBOR buffer into a JerryScript value.
 *
 * @param[in]  parser_allocator Allocator for memory needed by the parser, or NULL to use the default allocator
 * @param[in]  serialized       Buffer containig the CBOR data
 * @param[in]  serialized_size  Size of the serialized buffer
 * @param[out] value            JerryScript value created if the call succeeds
 *
 * @return FB_SMO_SUCCESS if the call succeeds, or FB_SMO_ERROR_XXX if it fails
 */
int Fb_Smo_Deserialize_CBOR_ToJerry(Fb_SmoAllocator* parser_allocator,
                                    const uint8_t*   serialized,
                                    unsigned int     serialized_size,
                                    jerry_value_t*   value);

/**
 * Deserialize a CBOR data source into a JerryScript value.
 *
 * @param[in]  parser_allocator Allocator for memory needed by the parser, or NULL to use the default allocator
 * @param[in]  data_source      The data source to read the CBOR data from
 * @param[out] value            JerryScript value created if the call succeeds
 *
 * @return FB_SMO_SUCCESS if the call succeeds, or FB_SMO_ERROR_XXX if it fails
 */
int Fb_Smo_Deserialize_CBOR_ToJerryFromSource(Fb_SmoAllocator*       parser_allocator,
                                              Fb_SmoJerryDataSource* data_source,
                                              jerry_value_t*         value);

/**
 * Custom encoder to use with Serialize_CBOR_FromJerry
 *
 * @param[in]    context    Optional context to use
 * @param[in]    object     Object to be encoded
 * @param[in]    serialized Array to be serialized into
 * @param[in]    available  Amount of available space in the array
 * @param[in]    result     Result of the encoding
 *
 * @return true if the object was encoded, false otherwise
 */
typedef bool (*Fb_Smo_Serialize_Cbor_Encoder)(void *context, jerry_value_t object, uint8_t** serialized, unsigned int* available, int* result);
    
/**
 * Serialize or measure the buffer size needed to serialize a JerryScript value to a CBOR buffer.
 *
 * @param[in]     value           JerryScript value that should be serialized
 * @param[in]     serialized      Buffer in which the serialized CBOR data will be written, or NULL to only get the serialized size
 * @param[in,out] serialized_size On input, the size in bytes of the buffer, or 0 if the buffer is NULL. On output, 
                                  the number of bytes unused at the end of the buffer after serialization, 
                                  or the number of bytes required for serialization if the buffer is NULL.
 * @param[in]     max_depth       Maximum nesting depth.
 * @param[in]     encoder         Optional encoder that will be used instead of the normal one.
 * @param[in,out] encoder_context Optional context to pass to the encoder.
 *
 * @return FB_SMO_SUCCESS if the call succeeds, FB_SMO_ERROR_NOT_ENOUGH_SPACE if the buffer is too small, 
           FB_SMO_ERROR_OVERLFOW if max_depth was exceeded or FB_SMO_ERROR_XXX for other error conditions.
 */
int Fb_Smo_Serialize_CBOR_FromJerry(jerry_value_t                 value,
                                    uint8_t*                      serialized,
                                    unsigned int*                 serialized_size,
                                    unsigned int                  max_depth,
                                    Fb_Smo_Serialize_Cbor_Encoder encoder,
                                    void*                         encoder_context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _FB_SMO_JERRYSCRIPT_H_ */
