#pragma once

#include "math/matrix.h"
#include "math/minimizer/gradient_minimizer.h"
#include "math/norm.h"

#include <cassert>

namespace Math
{

template <ObjectiveFn Fn>
void Minimize(const Fn & function,
              std::span<float> initial,
              const uint32_t iterationLimit,
              const float learningRate,
              const float tolerance)
{
	std::vector<float> grad;
	grad.resize(initial.size());

	for (std::size_t i = 0; i < iterationLimit; i++) {
		// Calculate the gradient
		function.Gradient(initial, grad);

		// Step opposite of the gradient
		for (std::size_t i = 0; i < initial.size(); i++) {
			initial[i] -= learningRate * grad[i];
		}

		// Check the gradient "size" - if we reached the local minimum
		const float norm = EuclideanNorm(grad);
		if (norm < tolerance) {
			break;
		}
	}
}

template <ObjectiveFn Fn>
void Gradient(const Fn & function,
              std::span<float> params,
              std::span<float> result,
              const float step)
{
	assert(params.size() == result.size());

	for (std::size_t i = 0; i < params.size(); i++) {
		const auto original = params[i];

		// f(x + step)
		params[i] = original + step;
		const auto upper = function(params);

		// f(x - step)
		params[i] = original - step;
		const auto lower = function(params);

		// Central difference
		result[i] = (upper - lower) / (2.0 * step);
		params[i] = original;
	}
}

}  // namespace Math
