#pragma once

#include "math/matrix.h"

#include <span>

namespace Math
{

/// @brief Class for objective function which calculates the error
/// between a point and several anchors.
class PointToAnchors
{
public:
	/// @brief Constructor.
	/// @param anchorMatrix cartesian positions of each of the anchors
	/// - N rows (anchors), M columns (dimensions - 2D/3D)
	/// @param distances distances between a point and each anchor
	/// - 1 row, M columns (dimensions - 2D/3D)
	/// No copy is made - the caller should make sure the data referenced by
	/// the span outlives this class.
	PointToAnchors(const Math::Matrix<float> & anchorMatrix, std::span<const float> distances);

	/// @brief Objective function
	/// @param point predicted value - 1 row, N columns (dimensions)
	/// @return error from the real value
	float operator()(std::span<const float> point) const;

	/// @brief Gradient of this function
	/// @param [in] point (x,y,z) coordinates of the current guess
	/// @param [out] gradient output gradient (3 dimensional)
	void Gradient(std::span<const float> point, std::span<float> gradient) const;

private:
	/// @brief Anchor matrix - cartesian positions of each of the anchors
	const Math::Matrix<float> & _anchorMatrix;

	/// @brief Distances - between a point and each anchor
	std::span<const float> _distances;
};

}  // namespace Math
