// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef SMART_PTR__H
#define SMART_PTR__H

class SmartPtrException {
public:
    SmartPtrException(const std::string& what): what_(what) {}
    const std::string& what() { return what_; }
private:
    std::string what_ ;
};

template < class T >
class SmartPtr { // 汎用 smart pointer
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

    template < class U >
    operator SmartPtr<U>() const { // 型変換
		U* u = dynamic_cast<U*>(pointee_); ////
		if( u == 0 )
			throw SmartPtrException("SmartPtr: failed to dynamic cast");
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

    T* pointee_; // 実体
};

class RCObject { // 参照回数計測用 class
public:

    RCObject() : counter_(0){}
    virtual ~RCObject(){}

    void refer() { ++counter_; }

    void release() {
		if( --counter_ == 0 )
			delete this;
    }

    int refCount() const { return (counter_); }

private:

    int counter_;
    RCObject( const RCObject& org );
    RCObject& operator = ( const RCObject& rhs );
};

#endif // SMART_PTR__H
