// -*- c++ -*-
// Authors: IWAI Jiro
//
// Legacy SmartPtr support (deprecated)
// This file is kept for backward compatibility only.
// All new code should use std::shared_ptr directly.
//
// Issue #45: SmartPtr/RCObjectの完全フェードアウト
// - RCObject class has been removed
// - SmartPtr template is disabled by default (NH_ENABLE_LEGACY_SMARTPTR=0)
// - Only SmartPtrException alias remains for backward compatibility

#ifndef SMART_PTR__H
#define SMART_PTR__H

#include <nhssta/exception.hpp>

#ifndef NH_ENABLE_LEGACY_SMARTPTR
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NH_ENABLE_LEGACY_SMARTPTR 0
#endif

// Backward compatibility: keep SmartPtrException as alias to Nh::RuntimeException
// This will be removed in a later phase when all code is migrated
using SmartPtrException = Nh::RuntimeException;

#if NH_ENABLE_LEGACY_SMARTPTR

template < class T >
class SmartPtr { // Legacy smart pointer (deprecated)
public:

    SmartPtr( T* pointee = 0 )
		: pointee_(pointee){ refer(); }

    SmartPtr( const SmartPtr& org )
		: pointee_(org.pointee_){ refer(); }

    virtual ~SmartPtr(){ release(); }

    SmartPtr& operator = ( const SmartPtr& rhs ){
		if( pointee_ == rhs.pointee_ )
			return (*this);
		release();
		pointee_ = rhs.pointee_;
		refer();
		return (*this);
    }

    bool operator == ( const SmartPtr& rhs ) const {
		return ( pointee_ == rhs.pointee_ );
    }

    bool operator != ( const SmartPtr& rhs ) const {
		return ( pointee_ != rhs.pointee_ );
    }

    virtual bool operator < ( const SmartPtr& rhs ) const {
		return ( pointee_ < rhs.pointee_ );
    }

    virtual bool operator > ( const SmartPtr& rhs ) const {
		return ( pointee_ > rhs.pointee_ );
    }

    T* operator -> () const { return (pointee_); }

    T& operator * () const { return (*pointee_); }

    T* get() const { return pointee_; }

    template < class U >
    operator SmartPtr<U>() const { // Dynamic cast
		U* u = dynamic_cast<U*>(pointee_);
		if( u == 0 )
			throw Nh::RuntimeException("SmartPtr: failed to dynamic cast");
		return SmartPtr<U>(u);
    }

private:

    void refer(){
		if( pointee_ )
			pointee_->refer();
    }

    void release(){
		if( pointee_ )
			pointee_->release();
    }

    T* pointee_; // Pointer
};

#endif // NH_ENABLE_LEGACY_SMARTPTR

// RCObject class has been removed - all classes now use std::shared_ptr directly
// This file now only contains SmartPtrException alias for backward compatibility
// and the legacy SmartPtr template (disabled by default)

#endif // SMART_PTR__H
