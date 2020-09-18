/*
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *        http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _TEST_EHSROUTER_H_
#define _TEST_EHSROUTER_H_

#if defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#include <oasys/debug/Log.h>
#include <oasys/thread/Mutex.h>
#include <oasys/util/App.h>
#include <oasys/util/Singleton.h>


namespace dtn {

class EhsExternalRouter;

/**
 * Main wrapper class for the Data Processing Procedure prototype (provider)
 */
class TestEhsExternalRouter : public oasys::App,
                              public oasys::Singleton<TestEhsExternalRouter>
{
public:
    /// Constructor
    TestEhsExternalRouter();

    /// Destructor
    ~TestEhsExternalRouter();

    /// Main application loop
    virtual int main(int argc, char* argv[]);

    /// Virtual from oasys::App
    virtual void fill_options();
    virtual void validate_options(int argc, char* const argv[], int remainder);

    virtual bool init_router();

    /// run time methods
    virtual int run();
    virtual void output_header_provider_mode();
    virtual void update_statistics();

    virtual void start_external_router();
    virtual bool parse_line(char* line);
    virtual void process_update_cfg();

    virtual void shutdown_server();
    virtual void bundle_stats();
    virtual void bundle_list();
    virtual void link_dump();
    virtual void send_recv_stats();
    virtual void fwdlink_transmit_dump();
    virtual void bundle_stats_by_src_dst();
    virtual void unrouted_bundle_stats_by_src_dst();
    virtual std::string fmt_bytes(uint64_t bytes);

    // callback method to return log messages to the application
    static void log_callback(const std::string& path, int level, 
                             const std::string& msg) {
        instance()->do_log_callback(path, level, msg);
    }

protected:
    // callback method to return log messages to the application
    virtual void do_log_callback(const std::string& path, int level, const std::string& msg);

protected:
    EhsExternalRouter* ehs_ext_router_;

    std::string config_file_;
    bool config_file_set_;

    bool user_mode_;
    bool user_abort_;

    bool do_stats_;
    bool link_stats_;

    bool manual_mode_;
};

} // namespace dpp


#endif // defined(XERCES_C_ENABLED) && defined(EXTERNAL_DP_ENABLED)

#endif /* _TEST_EHSROUTER_H_ */
