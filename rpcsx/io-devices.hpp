#pragma once

#include "audio/AudioDevice.hpp"
#include <cstdint>

namespace orbis {
struct IoDevice;
struct Process;
} // namespace orbis

orbis::IoDevice *createDceCharacterDevice();
orbis::IoDevice *createDipswCharacterDevice();
orbis::IoDevice *createDmemCharacterDevice(int index);
orbis::IoDevice *createGcCharacterDevice();
orbis::IoDevice *createHidCharacterDevice();
orbis::IoDevice *createHmd3daCharacterDevice();
orbis::IoDevice *createHmdCmdCharacterDevice();
orbis::IoDevice *createHmdMmapCharacterDevice();
orbis::IoDevice *createHmdSnsrCharacterDevice();
orbis::IoDevice *createNullCharacterDevice();
orbis::IoDevice *createZeroCharacterDevice();
orbis::IoDevice *createRngCharacterDevice();
orbis::IoDevice *createAjmCharacterDevice();
orbis::IoDevice *createIccConfigurationCharacterDevice();
orbis::IoDevice *createNpdrmCharacterDevice();
orbis::IoDevice *createConsoleCharacterDevice(int inputFd, int outputFd);
orbis::IoDevice *createSblSrvCharacterDevice();
orbis::IoDevice *createShmDevice();
orbis::IoDevice *createBlockPoolDevice();
orbis::IoDevice *createUrandomCharacterDevice();
orbis::IoDevice *createCameraCharacterDevice();
orbis::IoDevice *createNotificationCharacterDevice(int index);
orbis::IoDevice *createMBusCharacterDevice();
orbis::IoDevice *createBtCharacterDevice();
orbis::IoDevice *createXptCharacterDevice();
orbis::IoDevice *createCdCharacterDevice();
orbis::IoDevice *createMetaDbgCharacterDevice();
orbis::IoDevice *createHddCharacterDevice(std::uint64_t size);
orbis::IoDevice *createAoutCharacterDevice(std::int8_t id, AudioDevice *device);
orbis::IoDevice *createAVControlCharacterDevice();
orbis::IoDevice *createHDMICharacterDevice();
orbis::IoDevice *createMBusAVCharacterDevice();
orbis::IoDevice *createScaninCharacterDevice();
orbis::IoDevice *createS3DACharacterDevice();
orbis::IoDevice *createGbaseCharacterDevice();
orbis::IoDevice *createDevStatCharacterDevice();
orbis::IoDevice *createDevCtlCharacterDevice();
orbis::IoDevice *createDevActCharacterDevice();
orbis::IoDevice *createUVDCharacterDevice();
orbis::IoDevice *createVCECharacterDevice();
orbis::IoDevice *createEvlgCharacterDevice(int outputFd);
orbis::IoDevice *createSrtcCharacterDevice();
orbis::IoDevice *createScreenShotCharacterDevice();
orbis::IoDevice *createLvdCtlCharacterDevice();
orbis::IoDevice *createIccPowerCharacterDevice();
orbis::IoDevice *createCaymanRegCharacterDevice();
orbis::IoDevice *createA53IoCharacterDevice();
orbis::IoDevice *createNsidCtlCharacterDevice();
orbis::IoDevice *createHmd2CmdCharacterDevice();
orbis::IoDevice *createHmd2ImuCharacterDevice();
orbis::IoDevice *createHmd2GazeCharacterDevice();
orbis::IoDevice *createHmd2GenDataCharacterDevice();
