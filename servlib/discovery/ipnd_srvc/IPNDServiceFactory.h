/*
 *    Copyright 2012 Raytheon BBN Technologies
 *
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
 * 
 * IPNDServiceFactory.h
 *
 * This class is the heart of IPND Service Block entry creation and recognition.
 * This class is a singleton.
 */

#ifndef IPNDSERVICEFACTORY_H_
#define IPNDSERVICEFACTORY_H_

#include <oasys/debug/Logger.h>
#include <oasys/util/Singleton.h>

#include "discovery/IPNDService.h"

#include <list>
#include <istream>
#include <string>
#include <netinet/in.h>

namespace dtn {

class IPNDServiceFactory: public oasys::Singleton<IPNDServiceFactory,true>,
                          public oasys::Logger {
public:

    class Plugin: public oasys::Logger {
    public:
        /**
         * Configures and returns an immutable IPNDService definition based on
         * the given parameters. Returns NULL if this plugin does not understand
         * the given parameters.
         * Subclasses must implement.
         *
         * @param type The extracted type name of the requested service;
         *   subclasses should use for filtering efficiency
         * @param argc The number of parameters to process
         * @param argv The parameters to process
         * @return The configured IPNDService or NULL if the parameters are
         *   unknown to this plugin
         */
        virtual IPNDService *configureService(const std::string &type,
                const int argc, const char *argv[]) const = 0;

        /**
         * Tries to read a single service definition out of the given buffer.
         * This function updates the value of the num_read parameter to indicate
         * the number of bytes read from the buffer to create the returned
         * service. If this plugin does not recognize the service definition at
         * the given position, this function returns NULL and sets num_read to
         * zero. If this plugin recognizes the service definition but encounters
         * and error when reading it from the buffer, the return value will be
         * NULL, but num_read will be set to an error code out of IPNDSDTLV.
         *
         * @param tag The tag value that was peeked by the factory; subclasses
         *   should use this for filtering efficiency
         * @param buf Pointer to pointer to the buffer where the plugin will try
         *   to read a service definition
         * @param len_remain The remaining size of the buffer
         * @param num_read Used by the plugin to indicate the number of bytes
         *   read from the buffer or an error code
         */
        virtual IPNDService *readService(const uint8_t tag, const u_char **buf,
                const unsigned int len_remain, int *num_read) const = 0;

        virtual ~Plugin();
    protected:
        /**
         * Constructor to be used by subclasses.
         *
         * @param name Plugin name that will be used for logging
         */
        Plugin(const std::string name);

    };

    /**
     * Initializes the singleton instance
     */
    static void init() {
        if (instance_ != NULL) {
            PANIC("IPNDServiceFactory already initialized");
        }

        instance_ = new IPNDServiceFactory();
    }

    /**
     * Utilizes known plugins to configure an IPNDService with the given
     * parameters. Users are responsible for freeing the returned service
     * resources.
     *
     * See Plugin::configureService for detailed description.
     *
     * @return A service configured with the given parameters or NULL if
     *   parameters are unrecognized by all plugins
     */
    IPNDService *configureService(const int argc, const char *argv[]) const;

    /**
     * Utilizes known plugins to read IPNDServices out of the given stream. This
     * function reads out of the given stream until the given number of expected
     * services are read (or skipped) or the given length is exhausted (an error
     * condition if the number of expected services is not satisfied). Any
     * services that are recognized by any plugins are extracted
     * and added to the list that is eventually returned. If a service is
     * recognized by a plugin but is unable to be read due to an error, the
     * service is skipped and this function continues to the next one (if
     * applicable). If no services are recognized or read without error, the
     * empty vector is returned. This function updates the given stream so that
     * its get pointer is positioned immediately following the service block,
     * unless an unrecoverable error occurs, in which case the position of the
     * get pointer will be undefined.
     *
     * @param expected The expected number of services in the service block
     * @param in The stream to read services out of; get pointer positioned at
     *   the beginning of the first service definition
     * @param num_read Overwritten by the function to indicate the number of
     *   bytes read from the stream or an error code (see IPNDSDTLV)
     * @return List of services read from the stream; users take ownership of
     *   service resources
     */
    std::list<IPNDService*> readServices(const unsigned int expected,
            std::istream &in, int *num_read) const;

    /**
     * Adds the given plugin to the end of the plugins list. The factory takes
     * ownership of the plugin resources.
     *
     * Note that the factory automatically adds the default plugin.
     *
     * @param plugin Pointer to the plugin that will be added.
     */
    void addPlugin(IPNDServiceFactory::Plugin *plugin);

    virtual ~IPNDServiceFactory();

protected:

    /**
     * The list of plugins used to fulfill requests
     */
    std::list<IPNDServiceFactory::Plugin*> plugins_;
    typedef std::list<IPNDServiceFactory::Plugin*>::const_iterator PluginIter;

    /**
     * Should never be instantiated outside this class
     */
    IPNDServiceFactory();

private:
    friend class oasys::Singleton<IPNDServiceFactory, true>;

};

/**
 * This class defines the default plugin for configuring and extracting the
 * basic set of IPND services as defined in draft-irtf-dtnrg-ipnd-02.
 */
class DefaultIpndServicePlugin: public IPNDServiceFactory::Plugin {
public:
    DefaultIpndServicePlugin();
    virtual ~DefaultIpndServicePlugin();

    static const unsigned int MIN_PARAMS = 2;

    /**
     * See IPNDServiceFactory::Plugin
     */
    IPNDService *configureService(const std::string &type, const int argc,
            const char *argv[]) const;
    IPNDService *readService(const uint8_t tag, const u_char **buf,
                    const unsigned int len_remain, int *num_read) const;

private:
    bool parseOptions(const int argc, const char *argv[], in_addr_t *addr,
            uint16_t *port) const;
};

}

#endif /* IPNDSERVICEFACTORY_H_ */
