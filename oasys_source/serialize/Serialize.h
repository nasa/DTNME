/*
 *    Copyright 2004-2006 Intel Corporation
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
 */

#ifndef _OASYS_SERIALIZE_H_
#define _OASYS_SERIALIZE_H_

/**
 * @file
 * This file defines the core set of objects that define the
 * Serialization layer.
 */

#include <string>
#include <vector>
#include <sys/types.h>
#include <netinet/in.h>

#include "../compat/inttypes.h"
#include "../util/BufferCarrier.h"

namespace oasys {

class Serialize;
class SerializeAction;
class SerializableObject;

/**
 * Empty base class that's just used for name scoping of the action
 * and context enumerated types.
 */
class Serialize {
public:
    /**
     * Action type codes, one for each basic type of SerializeAction.
     */
    typedef enum {
        MARSHAL = 1,	///< in-memory  -> serialized representation
        UNMARSHAL,	///< serialized -> in-memory  representation
        INFO		///< informative scan (e.g. size, table schema)
    } action_t;
    
    /**
     * Context type codes, one for each general context in which
     * serialization occurs. 
     */
    typedef enum {
        CONTEXT_UNKNOWN = 1,	///< no specified context (default)
        CONTEXT_NETWORK,	///< serialization to/from the network
        CONTEXT_LOCAL		///< serialization to/from local disk
    } context_t;

    /** Options for un/marshaling. */
    enum {
        USE_CRC = 1 << 0,
    };

    /** Options for un/marshaling process() methods */
    enum {
        ALLOC_MEM       = 1<<0, ///< Allocated memory to be freed by the user 
        NULL_TERMINATED = 1<<1, ///< Delim by '\0' instead of storing length
    };
};

/**
 * Empty class used by various object factories (e.g. TypeCollection
 * and ObjectBuilder) to pass to the new object's constructor to
 * distinguish the call from a default construction.
 *
 * For example:
 *
 * @code
 * new SerializableObject(Builder());
 * @endcode
 */
class Builder {
public:
    Builder() {}
    Builder(const Builder&) {}

    static Builder& builder() {
        return static_builder_;
    }

protected:
    static Builder static_builder_;
};

/**
 * Utility class which enables different serializations
 * for in_addr_t and u_int32_t
 */
class InAddrPtr {
public:
    InAddrPtr(in_addr_t *addr)
        : addr_(addr) {}

    in_addr_t* addr() const { return addr_; }

protected:
    in_addr_t *addr_;
};

/**
 * Inherit from this class to add serialization capability to a class.
 */
class SerializableObject {
public:
    virtual ~SerializableObject() {}

    /**
     * This should call v->process() on each of the types that are to
     * be serialized in the object.
     */
    virtual void serialize(SerializeAction* a) = 0;
};

/**
 * A vector of SerializableObjects.
 */
typedef std::vector<SerializableObject*> SerializableObjectVector;

/**
 * The SerializeAction is responsible for implementing callback
 * functions for all the basic types. The action object is then passed
 * to the serialize() function which will re-dispatch to the basic
 * type functions for each of the SerializableObject's member fields.
 *
 * INVARIANT: A single SerializeAction must be able to be called on
 * several different visitee objects in succession. (Basically this
 * ability is used to be able to string several Marshallable objects
 * together, either for writing or reading).
 */
class SerializeAction : public Serialize {
public:
    
    /**
     * Create a SerializeAction with the specified type code and context
     *
     * @param action serialization action type code
     * @param context serialization context
     * @param options serialization options
     */
    SerializeAction(action_t action, context_t context, int options = 0);

    /**
     * Perform the serialization or deserialization action on the object.
     *
     * @return 0 if success, -1 on error
     */
    virtual int action(SerializableObject* object);

    /**
     * Control the initialization done before executing an action.
     */
    virtual void begin_action();

    /**
     * Control the cleanup after executing an action.
     */
    virtual void end_action();

    /**
     * Reset error flag to false.  Enables serializer reuse.
     */
    void reset_error() { error_ = false; }

    /**
     * Accessor for the action type.
     */
    action_t action_code() { return action_; }
    
    /**
     * Accessor for the context.
     */
    context_t context() { return context_; }

    /**
     * Accessor for error
     */ 
    bool error() { return error_; }

    /***********************************************************************
     *
     * Processor functions, one for each type.
     *
     ***********************************************************************/

    /**
     * Process function for a contained SerializableObject.
     *
     * The default implementation just calls serialize() on the
     * contained object, ignoring the name value. However, a derived
     * class can of course override it to make use of the name (as is
     * done by SQLTableFormat, for example).
     */
    virtual void process(const char* name, SerializableObject* object);

    /**
     * Process function for an 8 byte integer.
     */
    virtual void process(const char* name, u_int64_t* i) = 0;
    
    /**
     * Process function for a 4 byte integer.
     */
    virtual void process(const char* name, u_int32_t* i) = 0;

    /**
     * Process function for a 2 byte integer.
     */
    virtual void process(const char* name, u_int16_t* i) = 0;

    /**
     * Process function for a byte.
     */
    virtual void process(const char* name, u_int8_t* i) = 0;

    /**
     * Process function for a boolean.
     */
    virtual void process(const char* name, bool* b) = 0;

    /**
     * Process function for a constant length char buffer.
     * 
     * @param name   field name
     * @param bp     buffer
     * @param len    buffer length
     */
    virtual void process(const char* name, u_char* bp, u_int32_t len) = 0;

    /**
     * Process function for a variable length byte buffer.
     *
     * @param carrier Caller may want to check if they want to take
     * ownership of the buffer that is coming up from the
     * Serialization process !! -if it's possible- !!. This allows
     * zero copy semantics using underlying buffers if possible.
     *
     * @param lenp in/out - Length of the byte array.
     */
    virtual void process(const char*            name, 
                         BufferCarrier<u_char>* carrier) = 0;
   
    /*!
     * XXX/bowei document
     *
     * Process function for variable length byte arrays ending with
     * terminator. Byte array returned WILL ALWAYS be terminated with
     * the terminator.
     *
     * @param carrier Caller may want to check if they want to take
     * ownership of the buffer that is coming up from the
     * Serialization process !! -if it's possible- !!.This allows zero
     * copy semantics using underlying buffers if possible.
     *
     * @param lenp in - ignored, out - length of byte array.
     *
     * @param terminator What terminates the string. DO NOT put a
     * default parameter here!!!
     */
    virtual void process(const char*            name,
                         BufferCarrier<u_char>* carrier,
                         u_char                 terminator) = 0;

    /**
     * Process function for a c++ string.
     */
    virtual void process(const char* name, std::string* s) = 0;

    /*!
     * @{ Adaptor functions for signed/unsigned compatibility
     */
    virtual void process(const char* name, int64_t* i);    
    virtual void process(const char* name, int32_t* i);
    virtual void process(const char* name, int16_t* i);
    virtual void process(const char* name, int8_t* i);
    virtual void process(const char* name, char* bp, u_int32_t len);
    /// @}

#ifdef __CYGWIN__
    /*!
     * For some reason, Cygwin can't decide if an int should be signed
     * or unsigned so we need this adapter shim to disambiguate.
     */
    virtual void process(const char* name, int* i);
#endif


    //! @{ syntactic sugar for char buffers
    virtual void process(const char*          name, 
                         BufferCarrier<char>* carrier);
    
    virtual void process(const char*          name,
                         BufferCarrier<char>* carrier,
                         char                 terminator);
    //! @}

    /**
     * Default serialization of an in_addr_t
     * is to treat it as an integer
     */
    virtual void process(const char* name, const InAddrPtr& a);

    /** Set a log target for verbose serialization */
    void logpath(const char* log) { log_ = log; }
    
    /**
     * Destructor.
     */
    virtual ~SerializeAction();

protected:
    action_t  action_;	///< Serialization action code
    context_t context_;	///< Serialization context

    int       options_; ///< Serialization options
    const char* log_;	///< Optional log for verbose marshalling
    
    /**
     * Signal that an error has occurred.
     */
    void signal_error() { error_ = true; }
    
private:
    bool      error_;	///< Indication of whether an error occurred

    SerializeAction();	/// Never called
};

} // namespace oasys

#endif /* _OASYS_SERIALIZE_H_ */
