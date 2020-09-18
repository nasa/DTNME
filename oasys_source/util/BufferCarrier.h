#ifndef __BUFFERCARRIER_H__
#define __BUFFERCARRIER_H__

#include "../debug/DebugUtils.h"

namespace oasys {

/*!
 * This little wart of a class handles the case when a virtual
 * function interface can sometimes return a buffer without allocation
 * of memory but sometimes must, depending on the implementation of
 * the subclass.
 * 
 *
 */
template<typename _Type>
class BufferCarrier {
    NO_ASSIGN_COPY(BufferCarrier);

public:
    typedef _Type Type;
    
    /*!
     * Empty BufferCarrier, ready to recieve a buffer.
     */
    BufferCarrier()
        : buf_(0),
          len_(0),
          pass_ownership_(false)
    {}

    /*!
     * @param pass_ownership True iff you are passing ownership of the
     * buffer to this class.
     */
    BufferCarrier(_Type* buf, size_t len, bool pass_ownership)
        : buf_(buf), 
          len_(len),
          pass_ownership_(pass_ownership) 
    {}

    /*!
     * If we still own the buffer at the end of our life, then destroy
     * it when we die.
     */
    ~BufferCarrier() 
    { 
        if (pass_ownership_ && buf_ != 0) 
        { 
            free(buf_);
            reset();
        }
    }

    _Type* buf() const
    { 
        return buf_; 
    }

    size_t len() const
    {
        return len_;
    }
    
    bool pass_ownership() const
    { 
        return pass_ownership_;
    }

    bool is_empty() const
    {
        return buf_ == 0;
    }

    void set_buf(_Type* buf, size_t len, bool pass_ownership)
    {
        buf_ = buf;
        len_ = len;
        pass_ownership_ = pass_ownership;
    }
    
    void set_len(size_t len)
    {
        len_ = len;
    }

    /*!
     * Take the buffer from the carrier. After taking, the carrier is
     * reset to not hold anything and !! len() == 0 !!
     */
    _Type* take_buf(size_t* length) 
    { 
        if (pass_ownership_)
        {
            _Type* ret = buf_;
	    *length = len();

            reset();
            
            return ret;
        }
        else
        {
            if (buf_ == 0)
            {
		*length = len();
                reset();

                return 0;
            }
            else
            {
                _Type* new_buf = static_cast<_Type*>(malloc(sizeof(_Type) * len_));
                ASSERT(new_buf != 0);
                
                memcpy(new_buf, buf_, len_);
		*length = len();

                reset();

                return new_buf;
            }

        }
    }
        
    /*!
     * Reset BufferCarrier to not hold anything.
     */
    void reset()
    {
        buf_            = 0;
        pass_ownership_ = 0;
        len_            = 0;
    }
    

    /*!
     * Convert between different types of buffers. Note the len will
     * be messed up!
     */ 
    template<typename _OtherType>
    static void convert(BufferCarrier* out, const BufferCarrier<_OtherType>& in)
    {
        out->buf_ = reinterpret_cast<Type*>(in.buf());
        out->len_ = in.len();
        out->pass_ownership_ = in.pass_ownership();
    }
    
private:
    _Type* buf_;
    size_t len_;
    bool   pass_ownership_;
};

} // namespace oasys

#endif /* __BUFFERCARRIER_H__ */
