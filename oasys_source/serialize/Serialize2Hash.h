#ifndef __SERIALIZE2HASH_H__
#define __SERIALIZE2HASH_H__

#include "../util/ScratchBuffer.h"
#include "../compat/inttypes.h"

namespace oasys {

class SerializableObject;

class Serialize2Hash {
public:
    /*!
     * @param obj    Object to be hashed.
     */
    Serialize2Hash(const SerializableObject* obj);
    
    /*!
     * Returns a 32-bit hash.
     */
    u_int32_t get_hash32() const;
    
    /*!
     * Puts the MD5 hash in outbuf.
     */
    void get_hashMD5(const char* outbuf) const;
    
    // add more hashes as needed

private:
    const SerializableObject* obj_;
    ScratchBuffer<unsigned char*, 256> buf_;
};

} // namespace oasys

#endif /* __SERIALIZE2HASH_H__ */
