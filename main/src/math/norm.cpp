#include "math/norm.h"

#include <cmath>

namespace Math
{

float EuclideanNorm(std::span<const float> mat)
{
	return std::sqrt(EuclideanNormSqrd(mat));
}

float EuclideanNormSqrd(std::span<const float> mat)
{
	float sum = 0.0;
	for (const float value : mat) {
		sum += std::pow(value, 2);
	}
	return sum;
}

float EuclideanNorm(const Math::Matrix<float> & mat)
{
	return std::sqrt(EuclideanNormSqrd(mat));
}

float EuclideanNormSqrd(const Math::Matrix<float> & mat)
{
	float sum = 0.0;
	for (std::size_t i = 0; i < mat.Rows(); i++) {
		for (std::size_t j = 0; j < mat.Cols(); j++) {
			sum += std::pow(mat(i, j), 2);
		}
	}
	return sum;
}

float EuclideanDistance(const Math::Matrix<float> & mat, std::size_t row1, std::size_t row2)
{
	return std::sqrt(EuclideanDistanceSqrd(mat, row1, row2));
}

float EuclideanDistanceSqrd(const Math::Matrix<float> & mat, std::size_t row1, std::size_t row2)
{
	float sum = 0.0;
	for (std::size_t i = 0; i < mat.Cols(); i++) {
		sum += std::pow(mat(row1, i) - mat(row2, i), 2);
	}
	return sum;
}

}  // namespace Math