#include "math/minimizer/functions/anchor_distance.h"
#include "math/norm.h"

#include <cmath>

namespace Math
{

AnchorDistance3D::AnchorDistance3D(const Math::Matrix<float> & realDistances)
    : _realDistances(realDistances)
{
}

float AnchorDistance3D::operator()(std::span<const float> points) const
{
	assert(points.size() % 3 == 0);

	const std::size_t values = points.size() / 3;
	float sum = 0.0;
	for (std::size_t i = 0; i < values; i++) {
		const std::size_t iIdx = i * 3;
		for (std::size_t j = i + 1; j < values; j++) {
			if (_realDistances(i, j) == 0.0) {
				continue;  // Unknown
			}

			const std::size_t jIdx = j * 3;
			const float dist = std::pow(points[iIdx] - points[jIdx], 2)
			                   + std::pow(points[iIdx + 1] - points[jIdx + 1], 2)
			                   + std::pow(points[iIdx + 2] - points[jIdx + 2], 2);
			const float distance = std::sqrt(dist);
			sum += std::pow(distance - _realDistances(i, j), 2);
		}
	}
	return sum;
}

void AnchorDistance3D::Gradient(std::span<const float> points, std::span<float> gradient) const
{
	assert(points.size() % 3 == 0);
	assert(gradient.size() == points.size());

	const std::size_t values = points.size() / 3;
	for (std::size_t i = 0; i < values; i++) {
		const std::size_t iIdx = i * 3;
		const float xi = points[iIdx], yi = points[iIdx + 1], zi = points[iIdx + 2];

		gradient[iIdx] = gradient[iIdx + 1] = gradient[iIdx + 2] = 0.0;
		for (std::size_t j = i + 1; j < values; j++) {
			if (_realDistances(i, j) == 0.0) {
				continue;  // Unknown
			}

			const std::size_t jIdx = j * 3;
			const float xj = points[jIdx], yj = points[jIdx + 1], zj = points[jIdx + 2];

			const float dx = xi - xj;
			const float dy = yi - yj;
			const float dz = zi - zj;
			const float rn = std::sqrt(dx * dx + dy * dy + dz * dz);

			const float lhs = rn - _realDistances(i, j);
			gradient[iIdx] += lhs * (dx / rn);
			gradient[iIdx + 1] += lhs * (dy / rn);
			gradient[iIdx + 2] += lhs * (dz / rn);
		}
		gradient[iIdx] *= 2;
		gradient[iIdx + 1] *= 2;
		gradient[iIdx + 2] *= 2;
	}
}

}  // namespace Math
