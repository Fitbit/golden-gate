// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/remote/transport/serial/gg_remote_parser.h"
#include "xp/common/gg_results.h"

#include <string>

//----------------------------------------------------------------------
TEST_GROUP(GG_REMOTE)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

//----------------------------------------------------------------------
TEST(GG_REMOTE, Test_RemoteParser_Basic_FrameDetect) {
    // Scenario 1 -> Give a proper frame to parser and verify that
    // frame is correctly interpreted

    //------------- Positive test ---------------
    std::string sample_frame = "#o2ZwYXJhbXOhYXgCZm1ldGhvZGdjb3VudGVyYmlkGGU=$f3062b6b000001dc~";
    size_t frame_len = sample_frame.length();

    // Create parser and reset initial state
    GG_SerialRemoteParser parser;
    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == false);

    for (size_t i=0; i < frame_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_frame[i]);
    }

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == true);


    //------------- Negative test ----------------
    std::string sample_error_frame = "#o2ZwYXJhbXOhYXgCZm1ldGhvZGdjb3VudGVyYmlkGGU=$f3062b6b00000";
    frame_len = sample_error_frame.length();

    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == false);

    for (size_t i=0; i < frame_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_frame[i]);
    }

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == false);

    //-------------- Scenario----------------------
    // Frame engrossed in random bytes, check if we can parse the frame correctly
    std::string sample_frame_2 = "1654635#o2ZwYXJhbXOhYXgCZm1ldGhvZGdjb3VudGVyYmlkGGU=$f3062b6b000001dc~237465";
    frame_len = sample_frame_2.length(); // Actual frame is same as scenario 1

    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == false);

    for (size_t i=0; i < frame_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_frame_2[i]);
    }

    CHECK(GG_SerialRemoteParser_IsFrameReceived(&parser) == true);
}

//----------------------------------------------------------------------
TEST(GG_REMOTE, Test_RemoteParser_Basic_AckDetect) {
    // Scenario -> Give a proper ack frame to parser and verify that
    // ack frame is correctly interpreted

    //------------- Positive test ---------------
    std::string sample_ack_frame = "@12345678";
    size_t ack_len = sample_ack_frame.length();

    // Create parser and reset initial state
    GG_SerialRemoteParser parser;
    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == false);

    for (size_t i=0; i < ack_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_ack_frame[i]);
    }

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == true);

    //------------- Negative test ----------------
    std::string sample_ack_error_frame = "@1235678";
    ack_len = sample_ack_error_frame.length();

    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == false);
    for (size_t i=0; i < ack_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_ack_frame[i]);
    }

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == false);

    // ------------- Scenario ---------------------
    // ACK frame engrossed in random chars
    std::string sample_ack_frame_2 = "@@123456787346";
    ack_len = sample_ack_frame_2.length();
    GG_SerialRemoteParser_Reset(&parser);

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == false);
    for (size_t i=0; i < ack_len; ++i) {
        GG_SerialRemoteParser_PutData(&parser, (char)sample_ack_frame_2[i]);
    }

    CHECK(GG_SerialRemoteParser_IsAckReceived(&parser) == true);
}
