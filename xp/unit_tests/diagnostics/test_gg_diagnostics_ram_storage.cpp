#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_logging.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_system.h"
#include "xp/diagnostics/gg_diagnostics_ram_storage.h"

TEST_GROUP(GG_DIAGNOSTICS_RAM_STORAGE)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

#include <stdio.h>
TEST(GG_DIAGNOSTICS_RAM_STORAGE, Test_Diagnostics_RAMStorage) {
    GG_RAMStorage *storage;
    GG_Result result;
    uint8_t buf[128];
    uint8_t data[100];
    uint8_t *record;
    uint16_t buf_size;
    uint16_t data_len;
    uint16_t record_len;
    uint16_t handle3;
    uint16_t handle4;
    uint16_t offset;
    uint16_t count;
    uint8_t i;

    // Init record
    result = GG_Diagnostics_RAMStorage_Create(100, &storage);
    CHECK_EQUAL(GG_SUCCESS, result);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(0, count);

    // Test record that is too big
    data_len = 128;
    result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
    CHECK_TRUE(result != GG_SUCCESS);

    // Add 3 records
    data_len = 1;
    for (i = 0; i < 3; i++) {
        data[0] = i;
        result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(3, count);

    i = 0;
    handle3 = GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE;
    while (1) {
        buf_size = 4;
        result = GG_Diagnostics_RAMStorage_GetRecords(storage, &handle3, &buf_size, buf);
        CHECK_EQUAL(GG_SUCCESS, result);

        if (buf_size == 0) {
            break;
        }

        offset = 0;
        while (offset < buf_size) {
            memcpy(&record_len, &buf[offset], sizeof(record_len));
            offset += sizeof(record_len);
            record = &buf[offset];
            offset += record_len;

            CHECK_EQUAL(1, record_len);
            CHECK_EQUAL(i, record[0]);

            i++;
        }
    }

    CHECK_EQUAL(count, i);

    // Add 1 extra record and get handle
    data[0] = 3;
    result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
    CHECK_EQUAL(GG_SUCCESS, result);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(4, count);

    // Test remove records
    GG_Diagnostics_RAMStorage_DeleteRecords(storage, handle3);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(1, count);

    // Remove with invalid handle should have no effect on storage
    GG_Diagnostics_RAMStorage_DeleteRecords(storage, handle3);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(1, count);

    // Storage should have just '3' record
    handle4 = GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE;
    buf_size = 32;
    result = GG_Diagnostics_RAMStorage_GetRecords(storage, &handle4, &buf_size, buf);
    CHECK_EQUAL(GG_SUCCESS, result);

    memcpy(&record_len, buf, sizeof(record_len));
    record = &buf[sizeof(record_len)];

    CHECK_EQUAL(1, record_len);
    CHECK_EQUAL(3, record[0]);

    GG_Diagnostics_RAMStorage_DeleteRecords(storage, handle4);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(0, count);

    // Test adding records with removing old records
    for (i = 1; i < 8; i++) {
        data_len = i * 10;
        result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    // Test remove handle
    handle4 = GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE;
    do {
        buf_size = 100;
        result = GG_Diagnostics_RAMStorage_GetRecords(storage, &handle4, &buf_size, buf);
        CHECK_EQUAL(GG_SUCCESS, result);
    } while (buf_size != 0);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(0, count);

    // Test get records with gaps

    // Storage has size 100, so 100 / (2 + 10) = 8 records of size 10 should fit
    // without removing old ones
    data_len = 10;
    for (i = 0; i < 8; i++) {
        data[0] = i;
        result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    // Start iterating to get 1 record and then add 2 records. This should
    // create a gap as record with data[0] = 1 should be lost
    handle3 = GG_DIAGNOSTICS_RECORD_HANDLE_GENERATE;
    buf_size = 12;
    result = GG_Diagnostics_RAMStorage_GetRecords(storage, &handle3, &buf_size, buf);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(12, buf_size);
    CHECK_EQUAL(0, buf[2]);

    for (i = 8; i < 10; i++) {
        data[0] = i;
        result = GG_Diagnostics_RAMStorage_AddRecord(storage, data, data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    for (i = 2; i < 8; i++) {
        buf_size = 12;
        result = GG_Diagnostics_RAMStorage_GetRecords(storage, &handle3, &buf_size, buf);
        CHECK_EQUAL(GG_SUCCESS, result);
        CHECK_EQUAL(12, buf_size);
        CHECK_EQUAL(i, buf[2]);
    }

    // Remove records by handle; records 8 and 9 should still be in storage
    GG_Diagnostics_RAMStorage_DeleteRecords(storage, handle3);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(2, count);

    // Remove all records from storage
    GG_Diagnostics_RAMStorage_DeleteRecords(storage, GG_DIAGNOSTICS_RECORD_HANDLE_REMOVE);

    count = GG_Diagnostics_RAMStorage_GetRecordCount(storage);
    CHECK_EQUAL(0, count);

    GG_Diagnostics_RAMStorage_Destroy(storage);
}
