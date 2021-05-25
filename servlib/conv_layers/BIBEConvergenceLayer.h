/*
 *    Copyright
 * 
 */

#ifndef _BIBE_CONVERGENCE_LAYER_H_
#define _BIBE_CONVERGENCE_LAYER_H_

#ifdef HAVE_CONFIG_H
#    include <dtn-config.h>
#endif

#include <third_party/oasys/debug/Logger.h>
#include <third_party/oasys/util/ScratchBuffer.h>

#include "ConvergenceLayer.h"
#include "bundling/CborUtil.h"

namespace dtn {

/**
 * The Bundle In Bundle Encapsulation (BIBE) convergence layer 
 * implements the draft-ietf-dtn-bibect-03 (BPv7) and also allows
 * for encapsulating wiht BPv bundles.
 */
class BIBEConvergenceLayer : public ConvergenceLayer {
public:
    BIBEConvergenceLayer();
    ~BIBEConvergenceLayer() {}

    /// Link parameters
    class Params : public CLInfo {
    public:
        virtual ~Params() {}
        virtual void serialize(oasys::SerializeAction* a) override;

        uint32_t bp_version_ = 7;         // Which BP version bundles to generate (6 or 7)
        std::string dest_eid_;            // Where to send the new encapsulating bundle
        std::string src_eid_;             // Where to send the new encapsulating bundle
        bool custody_transfer_ = false;   // Whether encapsulation should use custody transfer

        // status only variables for easy reporting
        bool reserving_space_ = false; // Whether currently paused waiting for payload space

        // add options for BP6 ECOS?
    };

    /// Default parameters
    Params defaults_;

    /// @{ Virtual from ConvergenceLayer
    virtual bool interface_up(Interface* iface, int argc, const char* argv[]) override;
    virtual bool init_link(const LinkRef& link, int argc, const char* argv[]) override;
    virtual bool set_link_defaults(int argc, const char* argv[],
                           const char** invalidp) override;
    virtual bool reconfigure_link(const LinkRef& link, int argc, const char* argv[]) override;
    virtual void dump_link(const LinkRef& link, oasys::StringBuffer* buf) override;
    virtual void delete_link(const LinkRef& link) override;
    virtual bool open_contact(const ContactRef& contact) override;
    virtual bool close_contact(const ContactRef& contact) override;
    virtual void bundle_queued(const LinkRef& link, const BundleRef& bundle) override;
    virtual void cancel_bundle(const LinkRef& link, const BundleRef& bundle) override;

    /**
     * List valid options for links and interfaces
     */
    void list_link_opts(oasys::StringBuffer& buf) override;
    void list_interface_opts(oasys::StringBuffer& buf) override;

    /// @}

    /// Extract the bundle from a BIBE bundle
    static bool extract_bundle(Bundle* bibe_bundle);

protected:
    /*
     * Helper class that wraps the sender-side per-contact state.
     */
    class BIBE : public CLInfo, 
                 public Logger,
                 public oasys::Thread {
    public:
        /**
         * Destructor.
         */
        virtual ~BIBE();


       /**
        * Thread virtual run method
        */
        virtual void run() override;

    protected:
        /**
         * generate the encapsulation bundle
         */
        virtual void encapsulate_bundle(BundleRef& bref);

    private:
        friend class BIBEConvergenceLayer;

        /**
         * Constructor.
         */
        BIBE() = delete;
        BIBE(const ContactRef& contact, Params* params);
        
        /**
         * The contact that we're representing.
         */
        ContactRef contact_;

        /**
         * The Link object associated with this contact
         */
        LinkRef link_;

        /**
         * Pointer to the link parameters.
         */
        Params* params_;

        /**
         * work buffer
         */
        oasys::ScratchBuffer<uint8_t*> scratch_;

        /**
         * CBOR utility
         */
        CborUtil cborutil_;
    };   


private:
    /// Helper function to parse link parameters
    bool parse_link_params(Params* params,
                           int argc, const char** argv,
                           const char** invalidp);
    virtual CLInfo* new_link_params() override;
};

} // namespace dtn

#endif /* _BIBE_CONVERGENCE_LAYER_H_ */
