#pragma once

#include "matrix.h"

namespace
{

/// @brief Calculate index (row major)
/// @param r row
/// @param c column
/// @param tR total rows
/// @param tC total columns
/// @return index
constexpr std::size_t CalcIndex(std::size_t r, std::size_t c, std::size_t tR, std::size_t tC)
{
	return r * tC + c;
}

}  // namespace

namespace Math
{

template <typename T>
Matrix<T>::Matrix()
    : _rows(0)
    , _cols(0)
{
}

template <typename T>
Matrix<T>::Matrix(std::size_t rows, std::size_t cols)
    : _rows(rows)
    , _cols(cols)
{
	_data.resize(rows * cols);
}

template <typename T>
std::size_t Matrix<T>::Rows() const
{
	return _rows;
}

template <typename T>
std::size_t Matrix<T>::Cols() const
{
	return _cols;
}

template <typename T>
std::size_t Matrix<T>::Size() const
{
	return _data.size();
}

template <typename T>
std::span<T> Matrix<T>::Data()
{
	return _data;
}

template <typename T>
std::span<const T> Matrix<T>::Data() const
{
	return _data;
}

template <typename T>
std::span<float> Matrix<T>::Row(std::size_t idx)
{
	assert(idx < _rows);
	return std::span<float>(_data.data() + (idx * _cols), _cols);
}

template <typename T>
std::span<const float> Matrix<T>::Row(std::size_t idx) const
{
	assert(idx < _rows);
	return std::span<const float>(_data.data() + (idx * _cols), _cols);
}

template <typename T>
T Matrix<T>::operator()(std::size_t row, std::size_t col) const
{
	assert((row < _rows) && (col < _cols));
	return _data[CalcIndex(row, col, _rows, _cols)];
}

template <typename T>
T & Matrix<T>::operator()(std::size_t row, std::size_t col)
{
	assert((row < _rows) && (col < _cols));
	return _data[CalcIndex(row, col, _rows, _cols)];
}

template <typename T>
void Matrix<T>::Reshape(std::size_t rows, std::size_t cols)
{
	if ((rows == _rows) && (cols == _cols)) {  // no change
		return;
	}

	const std::size_t newSize = rows * cols;
	if ((newSize == _data.size()) || (rows == 0) || (cols == 0)) {  // change sizes
		_rows = rows;
		_cols = cols;
		return;
	}

	if (newSize < _data.size()) {
		// Reduce size
		if (_cols != cols) {
			// Move the data first
			std::size_t i = 0;
			for (std::size_t row = 0; row < rows; row++) {
				for (std::size_t col = 0; col < cols; col++) {
					_data[i++] = this->operator()(row, col);
				}
			}
		}
		_data.resize(newSize);  // should be free
	}
	else {
		// Increased size
		if (cols == _cols) {
			// Adding rows
			_data.resize(newSize);
		}
		else {
			// Adding columns / rows+columns
			_data.resize(newSize);
			for (std::size_t row = 0; row < rows; row++) {
				for (std::size_t col = 0; col < cols; col++) {
					// Start from the end - invert
					const std::size_t iRow = _rows - row - 1;
					const std::size_t iCol = _cols - col - 1;

					const std::size_t newIdx = CalcIndex(iRow, iCol, rows, cols);
					if (iCol <= _cols && iRow <= _rows) {
						// Map old positions to new ones
						_data[newIdx] = this->operator()(iRow, iCol);
					}
					else {
						_data[newIdx] = T{};
					}
				}
			}
		}
	}
	_rows = rows;
	_cols = cols;
}

template <typename T>
void Matrix<T>::AddRow(std::size_t idx)
{
	_data.resize(_cols * (_rows + 1));

	idx = std::clamp(idx, static_cast<std::size_t>(0), _rows - 1);
	if (idx != _rows - 1) {
		auto itRow = _data.begin() + (idx)*_cols;
		auto itRowNext = _data.begin() + (idx + 1) * _cols;
		std::copy(itRow, _data.end() - _cols, itRowNext);

		for (std::size_t i = 0; i < _cols; i++) {
			this->operator()(idx, i) = T{0};
		}
	}
	_rows++;
}

template <typename T>
void Matrix<T>::AddCol(std::size_t idx)
{
	Reshape(_rows, _cols + 1);  // I gave up
}

template <typename T>
void Matrix<T>::RemoveRow(std::size_t idx)
{
	idx = std::clamp(idx, static_cast<std::size_t>(0), _rows - 1);
	if (idx != _rows - 1) {
		// Move the data first
		auto itRow = _data.begin() + idx * _cols;
		auto itRowNext = _data.begin() + (idx + 1) * _cols;
		std::copy_backward(itRowNext, _data.end(), itRow);
	}
	_rows--;
	_data.resize(_cols * _rows);
}

template <typename T>
void Matrix<T>::RemoveCol(std::size_t idx)
{
	idx = std::clamp(idx, static_cast<std::size_t>(0), _cols - 1);

	// Shift
	std::size_t i = 0;
	for (std::size_t row = 0; row < _rows; row++) {
		for (std::size_t col = 0; col < _cols; col++) {
			if (col == idx) {
				continue;
			}
			_data[i++] = this->operator()(row, col);
		}
	}
	_cols--;
}

template <typename T>
void Matrix<T>::Fill(const T & value)
{
	std::fill(_data.begin(), _data.end(), value);
}

template <typename T>
void Matrix<T>::Reserve(std::size_t size)
{
	_data.reserve(size);
}

}  // namespace Math
