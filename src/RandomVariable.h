// -*- c++ -*-
// Author: IWAI Jiro

#ifndef RANDOM_VARIABLE__H
#define RANDOM_VARIABLE__H

#include <string>

#include "SmartPtr.h"
#include "Exception.h"

namespace RandomVariable {

	const double minimum_variance = 1.0e-6;

	// Backward compatibility: keep Exception as alias to Nh::RuntimeException
	// This will be removed in a later phase
	using Exception = Nh::RuntimeException;

	class _RandomVariable_;
	typedef SmartPtr<_RandomVariable_> RandomVariable;

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
