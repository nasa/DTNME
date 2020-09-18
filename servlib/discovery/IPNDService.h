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
 * IPNDService.h
 *
 * Defines an IPND Service (which is really no different than a generic
 * constructed type).
 */

#ifndef IPNDSERVICE_H_
#define IPNDSERVICE_H_

#include "ipnd_sd_tlv/ConstructedType.h"

#include <string>

namespace dtn
{

/**
 * This is really just an alias for ipndtlv::Constructed type. All IPND services
 * are constructed datatypes, but not all constructed datatypes are necessarily
 * IPND services.
 */
class IPNDService: public ipndtlv::ConstructedType {
protected:
    IPNDService(const uint8_t &tag_value, const std::string &name);
};

}

#endif /* IPNDService_H_ */
