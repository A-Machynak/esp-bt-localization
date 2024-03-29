#include "math/minimizer/functions/point_to_anchors.h"

#include <cmath>

namespace Math
{

PointToAnchors::PointToAnchors(const Math::Matrix<float> & anchorMatrix,
                               std::span<const float> distances)
    : _anchorMatrix(anchorMatrix)
    , _distances(distances)
{
}

float PointToAnchors::operator()(std::span<const float> point) const
{
	// Same amount of dimensions
	assert(_anchorMatrix.Cols() == point.size());

	const int anchorCount = _anchorMatrix.Rows();
	const int dimensions = point.size();

	float sum = 0.0;
	for (int i = 0; i < anchorCount; i++) {
		float error = 0.0;

		// Euclidean distance
		for (int j = 0; j < dimensions; j++) {
			const float diff = point[j] - _anchorMatrix(i, j);
			error += std::pow(diff, 2);
		}
		sum += (std::sqrt(error) - _distances[i]);
	}
	return sum;
}

void PointToAnchors::Gradient(std::span<const float> point, std::span<float> gradient) const
{
	assert(point.size() == gradient.size());

	gradient[0] = gradient[1] = gradient[2] = 0.0;
	for (int i = 0; i < _anchorMatrix.Rows(); i++) {
		const float dx = point[0] - _anchorMatrix(i, 0);
		const float dy = point[1] - _anchorMatrix(i, 1);
		const float dz = point[2] - _anchorMatrix(i, 2);
		const float rn = std::sqrt(dx * dx + dy * dy + dz * dz);

		const float lhs = 2.0 * (rn - _distances[i]);
		gradient[0] += lhs * (dx / rn);
		gradient[1] += lhs * (dy / rn);
		gradient[2] += lhs * (dz / rn);
	}
}

}  // namespace Math
