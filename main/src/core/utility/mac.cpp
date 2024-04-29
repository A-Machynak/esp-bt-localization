
#include "core/utility/mac.h"

#include <algorithm>

Mac::Mac()
    : Addr({0})
{
}

Mac::Mac(std::span<const std::uint8_t, Mac::Size> address)
{
	std::copy_n(address.data(), Mac::Size, Addr.begin());
}

bool Mac::operator==(const Mac & other) const
{
	return std::equal(Addr.begin(), Addr.end(), other.Addr.begin());
}

bool Mac::Cmp::operator()(const Mac & lhs, const Mac & rhs) const
{
	for (std::size_t i = 0; i < lhs.Addr.size(); i++) {
		if (lhs.Addr[i] < rhs.Addr[i])
			return true;
		if (lhs.Addr[i] > rhs.Addr[i])
			return false;
	}
	return false;  // Eq
}

std::string ToString(const Mac & addr)
{
	return ToString(std::span(addr.Addr));
}

std::string ToString(std::span<const std::uint8_t, Mac::Size> addr)
{
	static constexpr char ch[] = "0123456789ABCDEF";
	std::array<char, Mac::Size * 3> buffer;

	// 01:34:67:9A:CD:F0
	for (int i = 0; i < Mac::Size; i++) {
		const std::size_t offset = i * 3;
		buffer[offset] = ch[(addr[i] >> 4) & 0x0F];
		buffer[offset + 1] = ch[addr[i] & 0x0F];
		buffer[offset + 2] = ':';
	}
	buffer.back() = '\0';
	return std::string(buffer.data());
}
