#pragma once

#include <concepts>
#include <cstdint>
#include <span>

namespace Math
{

constexpr std::uint32_t DefaultIterationLimit = 1000;
constexpr float DefaultLearningRate = 0.1;
constexpr float DefaultTolerance = 1e-6;
constexpr float DefaultGradientStep = 1e-6;

template <typename T>
concept ObjectiveFn = requires(T fn) {
	{
		fn(std::span<const float>{})
	} -> std::convertible_to<float>;
	fn.Gradient(std::span<const float>{}, std::span<float>{});
};

/// @brief Minimizes a function using gradient descent.
/// @tparam Fn type of the function to minimize
/// @param function function to minimize
/// @param initial matrix for storing the result; contains the initial guess of the result
/// the result is a NxD matrix, where N is the number of points and D is the number of dimensions
/// @param iterationLimit maximum iteration count
/// @param learningRate how far to go the direction of the gradient each step
/// @param tolerance when to end the iteration
template <ObjectiveFn Fn>
void Minimize(const Fn & function,
              std::span<float> initial,
              const std::uint32_t iterationLimit = DefaultIterationLimit,
              const float learningRate = DefaultLearningRate,
              const float tolerance = DefaultTolerance);

/// @brief Generic gradient. Calculates gradient of a function using the central difference formula.
/// f'(x) ~ (f(x + step) - f(x - step)) / (2*step)
/// @tparam Fn type of the function to minimize
/// @param function function to minimize
/// @param matrix matrix containing the current guess;
/// the matrix is changed during calculation, but is restored at the end
/// @param result matrix for storing the resulting gradient
/// @param step step of the gradient
template <ObjectiveFn Fn>
void Gradient(const Fn & function,
              std::span<float> matrix,
              std::span<float> result,
              const float step = DefaultGradientStep);

}  // namespace Math

#include "math/minimizer/gradient_minimizer.hpp"
