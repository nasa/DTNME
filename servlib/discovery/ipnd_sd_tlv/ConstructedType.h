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
 * ConstructedType.h
 *
 * Defines the constructed datatype abstract class shell for IPND-SD-TLV.
 */

#ifndef CONSTRUCTEDTYPE_H_
#define CONSTRUCTEDTYPE_H_

#include "IPNDSDTLV.h"
#include "PrimitiveType.h"
#include <map>
#include <string>

namespace ipndtlv {

class ConstructedType: public IPNDSDTLV::BaseDatatype {
public:

    /** Minimum length of any constructed datatype is four bytes */
    static const unsigned int MIN_LEN = 2 + PrimitiveType::MIN_LEN;

    /**
     * See IPNDSDTLV::BaseDatatype
     */
    unsigned int getLength() const;
    virtual unsigned int getMinLength() const;
    virtual int read(const u_char **buf, const unsigned int &len_remain);
    virtual int write(const u_char **buf,
            const unsigned int &len_remain) const;

    virtual ~ConstructedType();

protected:

    /**
     * This map contains the composition of the constructed datatype. Map keys
     * are the tag values of the respective datatype value. While subclasses
     * may modify this map directly, it is recommended to use the 'addChild'
     * function instead.
     */
    std::map<uint8_t, IPNDSDTLV::BaseDatatype*> components_;
    typedef std::map<uint8_t, IPNDSDTLV::BaseDatatype*>::const_iterator
            CompIterator;

    /**
     * Adds a child datatype to the composition of this datatype. Only one entry
     * of each unique type is allowed; duplicates will be overwritten (but NOT
     * deleted, so be careful!).
     *
     * @param type The datatype to add to the composition
     */
    void addChild(IPNDSDTLV::BaseDatatype *type);

    /**
     * Retrieves the child datatype for the given tag value.
     *
     * @return A pointer to the requested child datatype or NULL if the
     *   requested child does not exist
     */
    IPNDSDTLV::BaseDatatype *getChild(uint8_t tag) const;

    /**
     * Calculates the size (in bytes) of all of the components that make up this
     * datatype.
     *
     * @return The length in bytes of the datatype's components
     */
    virtual unsigned int getComponentsLength() const;

    /**
     * Constructor - Should never be directly instantiated.
     *
     * @param tag_value The tag value of this datatype
     * @param name Human-readable name for datatype (for logging only)
     */
    ConstructedType(const uint8_t &tag_value, const std::string &name);
};

}

#endif /* CONSTRUCTEDTYPE_H_ */
