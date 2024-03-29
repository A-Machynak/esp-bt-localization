#pragma once

#include "math/matrix.h"

#include <cstdint>
#include <span>

namespace Math
{
/// @brief Calculates the euclidean norm/2-norm of a matrix
/// @param mat matrix
/// @return euclidean norm
float EuclideanNorm(const Math::Matrix<float> & mat);
float EuclideanNormSqrd(const Math::Matrix<float> & mat);
float EuclideanNorm(std::span<const float> mat);
float EuclideanNormSqrd(std::span<const float> mat);

/// @brief Euclidean distance between points at certain indices in a matrix
/// @param mat matrix
/// @param row1 first point index
/// @param row2 second point index
/// @return euclidean distance
float EuclideanDistance(const Math::Matrix<float> & mat, std::size_t row1, std::size_t row2);
float EuclideanDistanceSqrd(const Math::Matrix<float> & mat, std::size_t row1, std::size_t row2);

} // namespace Math