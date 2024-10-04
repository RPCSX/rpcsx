#pragma once

#include "audio/AudioDevice.hpp"
#include <cstdint>

struct IoDevice;

IoDevice *createDceCharacterDevice();
IoDevice *createDipswCharacterDevice();
IoDevice *createDmemCharacterDevice(int index);
IoDevice *createGcCharacterDevice();
IoDevice *createHidCharacterDevice();
IoDevice *createHmd3daCharacterDevice();
IoDevice *createHmdCmdCharacterDevice();
IoDevice *createHmdMmapCharacterDevice();
IoDevice *createHmdSnsrCharacterDevice();
IoDevice *createNullCharacterDevice();
IoDevice *createZeroCharacterDevice();
IoDevice *createRngCharacterDevice();
IoDevice *createAjmCharacterDevice();
IoDevice *createIccConfigurationCharacterDevice();
IoDevice *createNpdrmCharacterDevice();
IoDevice *createConsoleCharacterDevice(int inputFd, int outputFd);
IoDevice *createSblSrvCharacterDevice();
IoDevice *createShmDevice();
IoDevice *createBlockPoolDevice();
IoDevice *createUrandomCharacterDevice();
IoDevice *createCameraCharacterDevice();
IoDevice *createNotificationCharacterDevice(int index);
IoDevice *createMBusCharacterDevice();
IoDevice *createBtCharacterDevice();
IoDevice *createXptCharacterDevice();
IoDevice *createCdCharacterDevice();
IoDevice *createMetaDbgCharacterDevice();
IoDevice *createHddCharacterDevice(std::uint64_t size);
IoDevice *createAoutCharacterDevice(std::int8_t id, AudioDevice *device);
IoDevice *createAVControlCharacterDevice();
IoDevice *createHDMICharacterDevice();
IoDevice *createMBusAVCharacterDevice();
IoDevice *createScaninCharacterDevice();
IoDevice *createS3DACharacterDevice();
IoDevice *createGbaseCharacterDevice();
IoDevice *createDevStatCharacterDevice();
IoDevice *createDevCtlCharacterDevice();
IoDevice *createDevActCharacterDevice();
IoDevice *createUVDCharacterDevice();
IoDevice *createVCECharacterDevice();
IoDevice *createEvlgCharacterDevice(int outputFd);
IoDevice *createSrtcCharacterDevice();
IoDevice *createScreenShotCharacterDevice();
IoDevice *createLvdCtlCharacterDevice();
IoDevice *createIccPowerCharacterDevice();
IoDevice *createCaymanRegCharacterDevice();
