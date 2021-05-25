
#ifndef _BIBE_EXTRACTOR_H_
#define _BIBE_EXTRACTOR_H_

#ifdef HAVE_CONFIG_H
#  include <dtn-config.h>
#endif

#include <third_party/oasys/debug/Log.h>
#include <third_party/oasys/thread/SpinLock.h>
#include <third_party/oasys/thread/Thread.h>
#include <third_party/oasys/util/StreamBuffer.h>

#include "Bundle.h"
#include "CborUtil.h"


namespace dtn {

class Bundle;
class Registration;

/**
 * Class to offload extraction of  bundles from Bundle In Bundle Encapsulation
 */
class BIBEExtractor : public oasys::Thread,
                      public oasys::Logger {

public:
    
    /**
     * Constructor 
     */
    BIBEExtractor();
                   
    /**
     * destructor.
     */
    ~BIBEExtractor();

    void run();

    void shutdown();

    void post(Bundle* bundle, Registration* reg);

    size_t get_max_queued() { return eventq_.max_size(); }

protected:

    struct BibeEvent {
        BundleRef     bref_;
        Registration* reg_;
    };

    void process_event(BibeEvent* event);

    bool extract_bundle(Bundle* bibe_bundle);

    bool handle_custody_transfer(Bundle* bibe_bundle, uint64_t transmission_id, uint8_t reason);

protected:

    oasys::MsgQueue<struct BibeEvent*> eventq_;

    CborUtil cborutil_;

    oasys::StreamBuffer recvbuf_;
};


} // namespace dtn

#endif /* _BIBE_EXTRACTOR_H_ */
