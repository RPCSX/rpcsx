#pragma once
#ifdef HAVE_CUBEB

#if defined(TTARGET_OS_IPHONE) || defined(TARGET_OS_SIMULATOR)
	#error "Cubeb cannot be built on iOS platform."
#endif

#include "Emu/Audio/audio_device_enumerator.h"

#include "cubeb/cubeb.h"

class cubeb_enumerator final : public audio_device_enumerator
{
public:
	cubeb_enumerator();
	~cubeb_enumerator() override;

	std::vector<audio_device> get_output_devices() override;

private:
	cubeb* ctx{};
#ifdef _WIN32
	bool com_init_success = false;
#endif
};

#endif // HAVE_CUBEB
