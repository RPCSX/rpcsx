#pragma once
#include <rx/types.hpp>

namespace stx
{
	template <typename T, bool Se, usz Align>
	class se_t;

	template <typename T>
	struct lazy;

	template <typename T>
	struct generator;
} // namespace stx

namespace utils
{
	struct serial;
}

template <typename T>
extern bool serialize(utils::serial& ar, T& obj);

#define USING_SERIALIZATION_VERSION(name) []()  \
{                                               \
	extern void using_##name##_serialization(); \
	using_##name##_serialization();             \
}()

#define GET_OR_USE_SERIALIZATION_VERSION(cond, name) [&]()                                                         \
{                                                                                                                  \
	extern void using_##name##_serialization();                                                                    \
	extern s32 get_##name##_serialization_version();                                                               \
	return (static_cast<bool>(cond) ? (using_##name##_serialization(), 0) : get_##name##_serialization_version()); \
}()

#define GET_SERIALIZATION_VERSION(name) []()         \
{                                                    \
	extern s32 get_##name##_serialization_version(); \
	return get_##name##_serialization_version();     \
}()

#define ENABLE_BITWISE_SERIALIZATION using enable_bitcopy = std::true_type;
#define SAVESTATE_INIT_POS(...) static constexpr double savestate_init_pos = (__VA_ARGS__)
