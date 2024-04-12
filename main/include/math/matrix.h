#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace Math
{

/// @brief Matrix class. Meant to be used as a 2D vector, since it doesn't really provide
/// any matrix operations.
/// Row-major order:
/// [ 0 1 2 ]
/// [ 3 4 5 ]
/// [ 6 7 8 ]
/// @tparam T value types
template <typename T>
class Matrix
{
public:
	Matrix();
	Matrix(std::size_t rows, std::size_t cols);

	/// @brief Matrix parameters.
	/// @return count of rows/columns or total size
	/// @{
	std::size_t Rows() const;
	std::size_t Cols() const;
	std::size_t Size() const;
	/// @}

	/// @brief Raw data getter. Row-major.
	/// @return data
	/// @{
	std::span<T> Data();
	std::span<const T> Data() const;
	/// @}

	/// @brief Row getter
	/// @param idx index
	/// @return row
	/// @{
	std::span<float> Row(std::size_t idx);
	std::span<const float> Row(std::size_t idx) const;
	/// @}

	/// Can't get a column the same way; ignore it.
	// std::span<float> Col(std::size_t idx) const

	/// @brief Access op
	/// @param row row
	/// @param col column
	/// @return value
	/// @{
	T operator()(std::size_t row, std::size_t col) const;
	T & operator()(std::size_t row, std::size_t col);
	/// @}

	/// @brief Reshape the matrix while keeping the values
	/// @param rows new row count
	/// @param cols new column count
	void Reshape(std::size_t rows, std::size_t cols);

	/// @brief Add a single row/col in front of a row/col at index.
	/// @param idx index in range <0, Rows()/Cols()); values are clamped
	/// @{
	void AddRow(std::size_t idx = std::numeric_limits<std::size_t>::max());
	void AddCol(std::size_t idx = std::numeric_limits<std::size_t>::max());
	/// @}

	/// @brief Remove a single row/col
	/// @param idx index in range <0, Rows()/Cols()); values are clamped
	/// @{
	void RemoveRow(std::size_t idx);
	void RemoveCol(std::size_t idx);
	/// @}

	/// @brief Fill the matrix with value
	/// @param value value
	void Fill(const T & value);

	/// @brief Reserve for N values
	/// @param size size
	void Reserve(std::size_t size);

private:
	std::vector<T> _data;

	std::size_t _rows;
	std::size_t _cols;
};

}  // namespace Math

#include "math/matrix.hpp"
