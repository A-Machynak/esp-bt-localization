#include "master/http/api/post_data.h"

#include <algorithm>

namespace
{

/// @brief Calculate MacName value length. Expects the full PDU data (starts with 6B of MAC)
/// @param data MacName data (6B MAC + <1,16>B string ended with '\0')
/// @return value length <1,16>
static std::size_t MacNameValueLength(std::span<const std::uint8_t> data)
{
	using namespace Master::HttpApi::Type;
	assert((data.size() >= 7) && ((7 + 16) >= data.size()));

	constexpr std::size_t MacSize = 6;
	const std::size_t maxLength = std::min(MacName::MaxValueLength, data.size() - MacSize);

	const auto start = data.begin() + MacSize;
	const auto end = data.begin() + maxLength;
	const auto it = std::find_if(start, end, [](const std::uint8_t v) { return v == 0; });

	return (it == data.end()) ? maxLength : std::distance(start, it);
}

}  // namespace

namespace Master::HttpApi
{

DevicesPostDataView::DevicesPostDataView(std::span<const std::uint8_t> data)
    : Data(data)
{
}

PostDataEntry DevicesPostDataView::Next()
{
	if (Head >= Data.size()) {
		return PostDataEntry(std::monostate{});
	}

	if (Data[Head] > static_cast<std::uint8_t>(Type::ValueType::MacName)) {  // last entry
		return PostDataEntry(std::monostate{});
	}

	const Type::ValueType type = static_cast<Type::ValueType>(Data[Head]);
	std::span<const std::uint8_t> tData(Data.begin() + Head + 1, Data.end());

	switch (type) {
	case Type::ValueType::SystemMsg:
		if (Type::SystemMsg::IsValid(tData)) {
			Head += 2;
			return PostDataEntry(Type::SystemMsg(tData));
		}
		break;
	case Type::ValueType::RefPathLoss:
		if (Type::RefPathLoss::IsValid(tData)) {
			Head += 1 + decltype(Type::RefPathLoss::Data)::extent;
			return PostDataEntry(Type::RefPathLoss(tData));
		}
		break;
	case Type::ValueType::EnvFactor:
		if (Type::EnvFactor::IsValid(tData)) {
			Head += 1 + decltype(Type::EnvFactor::Data)::extent;
			return PostDataEntry(Type::EnvFactor(tData));
		}
		break;
	case Type::ValueType::MacName:
		if (Type::MacName::IsValid(tData)) {
			// Variable length <1,16>
			const Type::MacName p(tData);
			Head += 1 + p.ValueLength;
			return PostDataEntry(p);
		}
		break;
	}
	return PostDataEntry(std::monostate{});
}

namespace Type
{

SystemMsg::SystemMsg(std::span<const std::uint8_t> data)
    : Data(static_cast<Operation>(data[0]))
{
}

SystemMsg::Operation SystemMsg::Value() const
{
	return Data;
}

bool SystemMsg::IsValid(std::span<const std::uint8_t> data)
{
	return data.size() == 1;
}

RefPathLoss::RefPathLoss(std::span<const std::uint8_t> data)
    : Data(data)
{
}

std::span<const std::uint8_t, 6> RefPathLoss::Mac() const
{
	return std::span<const std::uint8_t, 6>(Data.data(), 6);
}

std::int8_t RefPathLoss::Value() const
{
	return Data[6];
}

bool RefPathLoss::IsValid(std::span<const std::uint8_t> data)
{
	return data.size() >= 7;
}

EnvFactor::EnvFactor(std::span<const std::uint8_t> data)
    : Data(data)
{
}

std::span<const std::uint8_t, 6> EnvFactor::Mac() const
{
	return std::span<const std::uint8_t, 6>(Data.data(), 6);
}

float EnvFactor::Value() const
{
	return *reinterpret_cast<const float *>(Data.data() + 6);
}

bool EnvFactor::IsValid(std::span<const std::uint8_t> data)
{
	return data.size() >= 10;
}

MacName::MacName(std::span<const std::uint8_t> data)
    : Data(data)
    , ValueLength(MacNameValueLength(data))
{
	assert((MinSize <= data.size()) && (data.size() <= MaxSize));
}

std::span<const std::uint8_t, 6> MacName::Mac() const
{
	return std::span<const std::uint8_t, 6>(Data.data(), 6);
}

std::span<const char> MacName::Value() const
{
	const auto it =
	    std::find_if(Data.begin() + 6, Data.end(), [](const std::uint8_t v) { return v == 0; });

	const std::size_t strSize =
	    (it == Data.end()) ? (Data.size() - 6) : (std::distance(Data.begin() + 6, it) - 1);

	return std::span<const char>(reinterpret_cast<const char *>(Data.data() + 6), strSize);
}

bool MacName::IsValid(std::span<const std::uint8_t> data)
{
	return (data.size() >= 8) && (data[0] != 0);
}

}  // namespace Type

}  // namespace Master::HttpApi
