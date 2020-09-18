#ifndef __STLUTIL_H__
#define __STLUTIL_H__

namespace oasys {

/*!
 * This is so sweet.
 *     - bowei
 *
 * Usage:
 *
 * CommaPushBack pusher(v);
 * pusher = pusher, 1, 2, 5, 6;
 * 
 */
template<typename _Vector, typename _Type>
class CommaPushBack {
public:
    CommaPushBack(_Vector* v)
        : v_(v) 
    {}
    
    CommaPushBack operator,(_Type type) {
        v_->push_back(type);
        return *this;
    }
                             
private:
    _Vector* v_;
};

} // namespace oasys

#endif /* __STLUTIL_H__ */
