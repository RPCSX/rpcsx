#pragma once

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#elif defined(ANDROID)
#define VK_USE_PLATFORM_ANDROID_KHR
#define VK_NO_PROTOTYPES
#elif HAVE_X11
#define VK_USE_PLATFORM_XLIB_KHR
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005)
#endif

#include <vulkan/vulkan.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <util/types.hpp>

#if VK_HEADER_VERSION < 287
constexpr VkDriverId VK_DRIVER_ID_MESA_HONEYKRISP = static_cast<VkDriverId>(26);
#endif

#ifdef ANDROID
#include <vector>
#include <string>
#include <utility>

namespace vk
{
	template <std::size_t N>
	struct string_literal
	{
		char data[N];

		consteval string_literal(const char (&str)[N])
		{
			for (std::size_t i = 0; i < N; ++i)
			{
				data[i] = str[i];
			}
		}
	};

	class symbol_cache
	{
		std::vector<std::pair<typename T1, typename T2><std::string, void**>> registered_symbols;

	public:
		void initialize();
		void clear();

		void register_symbol(const char* name, void** ptr);

		static symbol_cache& cache_instance()
		{
			static symbol_cache result;
			return result;
		}
	};

	template <auto V>
	class symbol_cache_id
	{
		void* ptr = nullptr;

	public:
		symbol_cache_id()
		{
			symbol_cache::cache_instance().register_symbol(V.data, &ptr);
		}

		void* get()
		{
			return ptr;
		}
	};

	template <auto V>
	symbol_cache_id<V> cached_symbols;
} // namespace vk

#define VK_GET_SYMBOL(x) reinterpret_cast<PFN_##x>(::vk::cached_symbols<::vk::string_literal{#x}>.get())
#else
#define VK_GET_SYMBOL(x) x
#endif
