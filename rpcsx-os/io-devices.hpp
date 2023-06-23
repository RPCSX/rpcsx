#pragma once

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
IoDevice *createStderrCharacterDevice();
IoDevice *createStdinCharacterDevice();
IoDevice *createStdoutCharacterDevice();
IoDevice *createZeroCharacterDevice();
IoDevice *createRngCharacterDevice();
