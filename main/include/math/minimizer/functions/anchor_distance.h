#pragma once

#include "math/matrix.h"

#include <span>

namespace Math
{

/// @brief Class for objective function which calculates the sum of
/// the squared distances between the observed values (`realDistances`)
/// and the predicted values.
class AnchorDistance3D
{
public:
	/// @brief Constructor.
	/// @param realDistances observed values; this is an upper triangular
	/// matrix representing distances between each of the values.
	/// @param dimensions dimension count
	AnchorDistance3D(const Math::Matrix<float> & realDistances);

	/// @brief Objective function
	/// @param points predicted values.
	/// Represented as a contiguous memory with 3 values (x,y,z).
	/// That is: { [x0], [y0], [z0], [x1], [y1], [z1], ... }
	/// @return sum of squared distances
	float operator()(std::span<const float> points) const;

	/// @brief Gradient of this function
	/// @param [in] point array of (x,y,z) coordinates of the current guess
	/// @param [out] gradient output gradient (3 dimensional)
	void Gradient(std::span<const float> points, std::span<float> gradient) const;

private:
	/// Observed values
	const Math::Matrix<float> & _realDistances;

	/// Dimension count
	std::size_t _dimensions;
};

}  // namespace Math
