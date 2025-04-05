#include "Utilities/File.h"
#include "Utilities/StrFmt.h"
#include "Utilities/Thread.h"
#include "dev/block_dev.hpp"
#include "dev/iso.hpp"
#include <filesystem>

[[noreturn]] void thread_ctrl::emergency_exit(std::string_view reason)
{
	std::fprintf(stderr, "%s\n", std::string(reason).c_str());
	std::abort();
}

int main(int argc, const char* argv[])
{
	auto file = fs::file(argv[1]);
	if (!file)
	{
		std::fprintf(stderr, "failed to open: %s (%s)\n", argv[1], fmt::format("%s", fs::g_tls_error).c_str());
		return 1;
	}

	auto iso = *iso_dev::open(std::make_unique<file_block_dev>(std::move(file)));

	if (argc <= 2)
	{
		return 0;
	}

	if (auto rawDir = iso.open_dir(argv[2]))
	{
		fs::dir dir;
		dir.reset(std::move(rawDir));
		for (auto& entry : dir)
		{
			std::printf("%s - %zd\n", entry.name.c_str(), entry.size);
		}

		return 0;
	}

	if (auto rawFile = iso.open(argv[2], fs::read))
	{
		fs::file file;
		file.reset(std::move(rawFile));
		auto name = std::filesystem::path(argv[2]).filename().string();
		fs::write_file(name, fs::create + fs::trunc, file.to_vector<std::uint8_t>());
	}

	return 1;
}
