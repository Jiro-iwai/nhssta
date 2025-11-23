// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef SMART_PTR__H
#define SMART_PTR__H

#include "exception.hpp"

#ifndef NH_ENABLE_LEGACY_SMARTPTR
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NH_ENABLE_LEGACY_SMARTPTR 0
#endif

// Backward compatibility: keep SmartPtrException as alias to Nh::RuntimeException
// This will be removed in a later phase
using SmartPtrException = Nh::RuntimeException;

#if NH_ENABLE_LEGACY_SMARTPTR

template < class T >
class SmartPtr { // ���� smart pointer
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
    operator SmartPtr<U>() const { // ���Ѵ�
		U* u = dynamic_cast<U*>(pointee_); ////
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

    T* pointee_; // ����
};

#endif // NH_ENABLE_LEGACY_SMARTPTR

class RCObject { // ���Ȳ����¬�� class
public:

    RCObject() = default;
    virtual ~RCObject() = default;

    void refer() { ++counter_; }

    void release() {
		if( --counter_ == 0 ) {
			delete this;
		}
    }

    [[nodiscard]] int refCount() const { return (counter_); }

    RCObject( const RCObject& org ) = delete;
    RCObject& operator = ( const RCObject& rhs ) = delete;

private:

    int counter_ = 0;
};

#endif // SMART_PTR__H
