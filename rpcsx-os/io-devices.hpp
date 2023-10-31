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
IoDevice *createIccConfigurationCharacterDevice();
IoDevice *createNpdrmCharacterDevice();
IoDevice *createConsoleCharacterDevice();
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
