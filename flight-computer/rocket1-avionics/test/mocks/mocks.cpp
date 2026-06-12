// =============================================================================
// test/mocks/mocks.cpp — global instances for the mock environment
// =============================================================================
#include "Arduino.h"
#include "SD.h"
#include "SPI.h"
#include "Wire.h"
#include "mock_state.h"

MockState g_mock;
MockSerial Serial;
TwoWire Wire;
SPIClass SPI;
SDClass SD;
