#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_strings.h"
#include "xp/common/gg_system.h"
#include "xp/utils/gg_data_probe.h"

TEST_GROUP(GG_DATA_PROBE)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    GG_IMPLEMENTS(GG_DataProbeListener);
    uint32_t           options;
    GG_Timestamp       time;
    uint32_t           expected_calculation;
    uint32_t           expected_calculation_peak;
    uint32_t           num_reports;
} Test_DataProbeListener;

static void
Test_DataProbeListener_OnReportReady(GG_DataProbeListener* _self, GG_DataProbe* probe)
{
    Test_DataProbeListener* self = GG_SELF(Test_DataProbeListener, GG_DataProbeListener);
    const GG_DataProbeReport* report = GG_DataProbe_GetReportWithTime(probe, self->time);
    if (self->options & GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT) {
        CHECK_EQUAL(self->expected_calculation, report->total_throughput);
        CHECK_EQUAL(self->expected_calculation_peak, report->total_throughput_peak);
    }
    if (self->options & GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT) {
        CHECK_EQUAL(self->expected_calculation, report->window_throughput);
        CHECK_EQUAL(self->expected_calculation_peak, report->window_throughput_peak);
    }
    if (self->options & GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL) {
        CHECK_EQUAL(self->expected_calculation, report->window_bytes_second);
        CHECK_EQUAL(self->expected_calculation_peak, report->window_bytes_second_peak);
    }
    ++self->num_reports;
}

GG_IMPLEMENT_INTERFACE(Test_DataProbeListener, GG_DataProbeListener) {
    .OnReportReady = Test_DataProbeListener_OnReportReady,
};

static void
Test_DataProbeListenerInit(Test_DataProbeListener* self, uint32_t options) {
    GG_SET_INTERFACE(self, Test_DataProbeListener, GG_DataProbeListener);
    self->options = options;
    self->num_reports = 0;
}

TEST(GG_DATA_PROBE, Test_Intregral) {
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    const GG_DataProbeReport* report;

    GG_DataProbe* probe;
    GG_DataProbe_Create(GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL,
                        500,
                        5000,
                        0,
                        NULL,
                        &probe);

    GG_DataProbe_ResetWithTime(probe, now);

    GG_DataProbe_AccumulateWithTime(probe, 500,  (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(0, report->window_bytes_second);
    CHECK_EQUAL(0, report->window_bytes_second_peak);

    GG_DataProbe_AccumulateWithTime(probe, 1500, (now + ((uint64_t)3 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 700,  (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(2000, report->window_bytes_second);
    CHECK_EQUAL(2000, report->window_bytes_second_peak);

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)8 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4300, report->window_bytes_second);
    CHECK_EQUAL(4300, report->window_bytes_second_peak);

    // Check case where sample lines up with beginning of window
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)9 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(3500, report->window_bytes_second);
    CHECK_EQUAL(4300, report->window_bytes_second_peak);

    GG_DataProbe_AccumulateWithTime(probe, 1200, (now + ((uint64_t)9 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 1600, (now + ((uint64_t)11 * GG_NANOSECONDS_PER_SECOND)));

    // Make sure old sample is preserved for interpolation
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)11 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4500, report->window_bytes_second);
    CHECK_EQUAL(4500, report->window_bytes_second_peak);

    GG_DataProbe_Destroy(probe);
}

TEST(GG_DATA_PROBE, Test_SquashIntegral) {
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    const GG_DataProbeReport* report;

    GG_DataProbe* probe;
    GG_DataProbe_Create(GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL,
                        3,
                        3500,
                        0,
                        NULL,
                        &probe);

    GG_DataProbe_ResetWithTime(probe, now);

    // Add 3 samples to fill up the buffer
    GG_DataProbe_AccumulateWithTime(probe, 500, now);
    GG_DataProbe_AccumulateWithTime(probe, 1500, (now + ((uint64_t)1 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 500, (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(2000, report->window_bytes_second);
    CHECK_EQUAL(2000, report->window_bytes_second_peak);

    // Ask for a report in the future
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)3 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(2500, report->window_bytes_second);
    CHECK_EQUAL(2500, report->window_bytes_second_peak);

    // Add a new sample which should trigger squash of second sample
    // and verify previous report is not be affected
    GG_DataProbe_AccumulateWithTime(probe, 1500, (now + ((uint64_t)3 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)3 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(2500, report->window_bytes_second);
    CHECK_EQUAL(2500, report->window_bytes_second_peak);

    // Ask for a report in the future greater than our window, which makes the first sample be
    // ouside of the window. Only half of the first sample value should contribute to overall byte
    // second calculation
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(3750, report->window_bytes_second);
    CHECK_EQUAL(3750, report->window_bytes_second_peak);

    // Add a new sample which should trigger another squash of the second sample
    GG_DataProbe_AccumulateWithTime(probe, 2000, (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));

    // Verify asking for old report is not be affected
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(3750, report->window_bytes_second);
    CHECK_EQUAL(3750, report->window_bytes_second_peak);

    // Add a new sample which should push out the first sample out of the window, so asking for
    // a previous report would yield a different result now.
    GG_DataProbe_AccumulateWithTime(probe, 1000, (now + ((uint64_t)5 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(3500, report->window_bytes_second);
    CHECK_EQUAL(3750, report->window_bytes_second_peak);

    // Only a portion of first point (now squashed) is used for new report
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)5 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4916, report->window_bytes_second);
    CHECK_EQUAL(4916, report->window_bytes_second_peak);
}

TEST(GG_DATA_PROBE, Test_ConstantIngegral) {
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    const GG_DataProbeReport* report;

    GG_DataProbe* probe;
    GG_DataProbe_Create(GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL,
                        500,
                        4000,
                        0,
                        NULL,
                        &probe);

    GG_DataProbe_ResetWithTime(probe, now);

    GG_DataProbe_AccumulateWithTime(probe, 500, now);
    GG_DataProbe_AccumulateWithTime(probe, 1500, (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 500, (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4000, report->window_bytes_second);
    CHECK_EQUAL(4000, report->window_bytes_second_peak);

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)5 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4000, report->window_bytes_second);
    CHECK_EQUAL(4000, report->window_bytes_second_peak);

    GG_DataProbe_AccumulateWithTime(probe, 1500,  (now + ((uint64_t)6 * GG_NANOSECONDS_PER_SECOND)));
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)6 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4000, report->window_bytes_second);
    CHECK_EQUAL(4000, report->window_bytes_second_peak);

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)7 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(4000, report->window_bytes_second);
    CHECK_EQUAL(4000, report->window_bytes_second_peak);

    GG_DataProbe_Destroy(probe);
}

TEST(GG_DATA_PROBE, Test_WindowThroughputReporting) {
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    Test_DataProbeListener listener;
    Test_DataProbeListenerInit(&listener, GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT);

    GG_DataProbe* probe;
    GG_DataProbe_Create(GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT,
                        500,
                        5000,
                        2000,
                        GG_CAST(&listener, GG_DataProbeListener),
                        &probe);

    GG_DataProbe_ResetWithTime(probe, now);

    listener.time = (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND));
    listener.expected_calculation  = 100;
    listener.expected_calculation_peak = 100;
    GG_DataProbe_AccumulateWithTime(probe, 500,  (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(listener.num_reports, 1);

    listener.time = (now + ((uint64_t)5 * GG_NANOSECONDS_PER_SECOND));
    listener.expected_calculation  = 240;
    listener.expected_calculation_peak = 240;
    GG_DataProbe_AccumulateWithTime(probe, 700,  (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(listener.num_reports, 2);

    // Check that old sample is removed and that peak is maintained
    listener.time = (now + ((uint64_t)9 * GG_NANOSECONDS_PER_SECOND));
    listener.expected_calculation  = 200;
    listener.expected_calculation_peak = 240;
    GG_DataProbe_AccumulateWithTime(probe, 300,  (now + ((uint64_t)7 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(listener.num_reports, 3);

    // Check that update rate is enforced
    GG_DataProbe_AccumulateWithTime(probe, 450,  (now + ((uint64_t)10 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(listener.num_reports, 3);

    // Check that we get an update after not having one from the previous accumulate
    listener.time = (now + ((uint64_t)12 * GG_NANOSECONDS_PER_SECOND));
    listener.expected_calculation  = 240;
    listener.expected_calculation_peak = 240;
    GG_DataProbe_AccumulateWithTime(probe, 450,  (now + ((uint64_t)11 * GG_NANOSECONDS_PER_SECOND)));

    GG_DataProbe_Destroy(probe);
}

// no window test
TEST(GG_DATA_PROBE, Test_DataProbeNoWindow) {
    GG_Timestamp now = GG_System_GetCurrentTimestamp();
    const GG_DataProbeReport* report;

    GG_DataProbe* probe;
    GG_DataProbe_Create(GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT,
                        0,
                        0,
                        0,
                        NULL,
                        &probe);

    GG_DataProbe_ResetWithTime(probe, now);

    GG_DataProbe_AccumulateWithTime(probe, 500,  (now + ((uint64_t)2 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 1500, (now + ((uint64_t)3 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 700,  (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)4 * GG_NANOSECONDS_PER_SECOND)));
    CHECK_EQUAL(675, report->total_throughput);
    CHECK_EQUAL(675, report->total_throughput_peak);

    // Test spaced out data points and casting
    GG_DataProbe_AccumulateWithTime(probe, 6000, (now + ((uint64_t)5 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 372,  (now + ((uint64_t)9 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 45,   (now + ((uint64_t)12 * GG_NANOSECONDS_PER_SECOND)));

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)12 * GG_NANOSECONDS_PER_SECOND)));

    CHECK_EQUAL(759, report->total_throughput);
    CHECK_EQUAL(759, report->total_throughput_peak);

    // Test no data presented, and retention of peak throughput
    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)20 * GG_NANOSECONDS_PER_SECOND)));

    CHECK_EQUAL(455, report->total_throughput);
    CHECK_EQUAL(759, report->total_throughput_peak);

    // Test simultaneous reset with accumulate
    GG_DataProbe_ResetWithTime(probe, (now + ((uint64_t)21 * GG_NANOSECONDS_PER_SECOND)));
    GG_DataProbe_AccumulateWithTime(probe, 500,  (now + ((uint64_t)21 * GG_NANOSECONDS_PER_SECOND)));

    report = GG_DataProbe_GetReportWithTime(probe,
                                            (now + ((uint64_t)21 * GG_NANOSECONDS_PER_SECOND)));

    CHECK_EQUAL(0, report->total_throughput);
    CHECK_EQUAL(0, report->total_throughput_peak);

    GG_DataProbe_Destroy(probe);
}
