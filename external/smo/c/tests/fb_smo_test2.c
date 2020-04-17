/**
*
* @file: fb_smo_test1.c
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
* Simple Message Object Model - Tests 2
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fb_smo.h"
#include "fb_smo_serialization.h"

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    int result;
    uint8_t test_data[] = {0x00, 0x00};
    Fb_Smo* root = NULL;

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, test_data, sizeof(test_data), &root);

    return result;
}
