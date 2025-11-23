// -*- c++ -*-
// Author: IWAI Jiro

#ifndef RANDOM_VARIABLE__H
#define RANDOM_VARIABLE__H

#include <memory>
#include <string>
#include <type_traits>

#include "SmartPtr.h"
#include "Exception.h"

namespace RandomVariable {

	const double minimum_variance = 1.0e-6;

	// Backward compatibility: keep Exception as alias to Nh::RuntimeException
	// This will be removed in a later phase
	using Exception = Nh::RuntimeException;

	class _RandomVariable_;

	class RandomVariableHandle {
	public:
		using element_type = _RandomVariable_;

		RandomVariableHandle() = default;
		RandomVariableHandle(std::nullptr_t) : body_(nullptr) {}
		explicit RandomVariableHandle(_RandomVariable_* body)
			: body_(body ? std::shared_ptr<_RandomVariable_>(body) : nullptr) {}
		explicit RandomVariableHandle(std::shared_ptr<_RandomVariable_> body)
			: body_(std::move(body)) {}

		template <class Derived,
		          class = std::enable_if_t<std::is_base_of<_RandomVariable_, Derived>::value>>
		explicit RandomVariableHandle(const std::shared_ptr<Derived>& body)
			: body_(std::static_pointer_cast<_RandomVariable_>(body)) {}

		_RandomVariable_* operator->() const { return body_.get(); }
		_RandomVariable_& operator*() const { return *body_; }
		_RandomVariable_* get() const { return body_.get(); }
		std::shared_ptr<_RandomVariable_> shared() const { return body_; }
		explicit operator bool() const { return static_cast<bool>(body_); }

		bool operator==(const RandomVariableHandle& rhs) const {
			return body_.get() == rhs.body_.get();
		}
		bool operator!=(const RandomVariableHandle& rhs) const {
			return !(*this == rhs);
		}
		bool operator<(const RandomVariableHandle& rhs) const {
			return body_.get() < rhs.body_.get();
		}
		bool operator>(const RandomVariableHandle& rhs) const {
			return body_.get() > rhs.body_.get();
		}

		template <class U>
		std::shared_ptr<U> dynamic_pointer_cast() const {
			auto ptr = std::dynamic_pointer_cast<U>(body_);
			if (!ptr) {
				throw Nh::RuntimeException("RandomVariable: failed to dynamic cast");
			}
			return ptr;
		}

	private:
		std::shared_ptr<_RandomVariable_> body_;
	};

	using RandomVariable = RandomVariableHandle;

	class _RandomVariable_ : public RCObject {
	public:

		_RandomVariable_();
		virtual ~_RandomVariable_();

		const std::string& name() const;
		void set_name(const std::string& name);

		const RandomVariable& left() const;
		const RandomVariable& right() const;

		double mean();
		double variance();

		int level() const { return level_; }

	protected:

		_RandomVariable_
		(
			double mean,
			double variance,
			const std::string& name = ""
			);

		_RandomVariable_
		(
			const RandomVariable& left,
			const RandomVariable& right,
			const std::string& name = ""
			);

	protected:

		virtual double calc_mean() const;
		virtual double calc_variance() const;

		void check_variance(double& v) const;

		std::string name_;
		RandomVariable left_;
		RandomVariable right_;
		double mean_;
		double variance_;

		bool is_set_mean_;
		bool is_set_variance_;
		int level_;
	};

}

#endif // RANDOM_VARIABLE__H
