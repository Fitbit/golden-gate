#include <xp/common/gg_io.h>
#include <jni.h>

extern "C" {

// We need something on which putData can be called.
struct TxSink {
    GG_IMPLEMENTS(GG_DataSink);
    jobject receiver;
    jmethodID callback;
};

struct RxSource {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);

    GG_DataSink* sink;
};

}
