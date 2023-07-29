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
IoDevice *createZeroCharacterDevice();
IoDevice *createRngCharacterDevice();
IoDevice *createAjmCharacterDevice();
