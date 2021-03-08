#include "SPISlavePeripheral.h"

SPISlavePeripheral::SPISlavePeripheral(pin_type clk, pin_type miso, pin_type mosi, pin_type cs, uint8_t CPOL, uint8_t CPHA) : VirtualPrinter::Component("SPISlavePeripheral"), clk_pin(clk), miso_pin(miso), mosi_pin(mosi), cs_pin(cs), CPOL(CPOL), CPHA(CPHA) {
  Gpio::attach(clk_pin, [this](GpioEvent& event){ this->spiInterrupt(event); });
  Gpio::attach(cs_pin, [this](GpioEvent& event){ this->spiInterrupt(event); });
}

SPISlavePeripheral::~SPISlavePeripheral() {};

void SPISlavePeripheral::onBeginTransaction() {
  // printf("SPISlavePeripheral::onBeginTransaction\n");
  insideTransaction = true;
  outgoing_byte = 0xFF;
  outgoing_bit_count = 0;
  if (CPHA == 0) {
    //when CPHA is 0: data must be available on MISO when CS fall
    transmitCurrentBit();
  }
}

void SPISlavePeripheral::transmitCurrentBit() {
  // MSB
  const uint8_t b = (outgoing_byte >> (7 - outgoing_bit_count)) & 1;
  Gpio::set_pin_value(miso_pin,  b);
  onBitSent(b);
  if (++outgoing_bit_count == 8) {
    onByteSent(outgoing_byte);
  }
}

void SPISlavePeripheral::onEndTransaction() {
  // printf("SPISlavePeripheral::onEndTransaction\n");
  // check for pending data to receive
  if (requestedDataSize > 0) {
    onRequestedDataReceived(currentToken, requestedData, requestedDataIndex);
  }
  setRequestedDataSize(0xFF, 0);
  insideTransaction = false;
}

void SPISlavePeripheral::onBitReceived(uint8_t _bit) {
  // printf("SPISlavePeripheral::onBitReceived: %d\n", _bit);
}

void SPISlavePeripheral::onBitSent(uint8_t _bit) {
  // printf("SPISlavePeripheral::onBitSent: %d\n", _bit);
}

void SPISlavePeripheral::onByteReceived(uint8_t _byte) {
  // printf("SPISlavePeripheral::onByteReceived: %d\n", _byte);
  if (requestedDataSize > 0) {
    requestedData[requestedDataIndex++] = _byte;
    if (requestedDataIndex == requestedDataSize) {
      requestedDataSize = 0;
      onRequestedDataReceived(currentToken, requestedData, requestedDataIndex);
    }
  }
}

void SPISlavePeripheral::onResponseSent() {
  // printf("SPISlavePeripheral::onResponseSent\n");
  hasDataToSend = false;
}

void SPISlavePeripheral::onRequestedDataReceived(uint8_t token, uint8_t* _data, size_t count) {
  // printf("SPISlavePeripheral::onRequestedDataReceived\n");
}

void SPISlavePeripheral::onByteSent(uint8_t _byte) {
  // printf("SPISlavePeripheral::onByteSent: %d\n", _byte);
  if (responseDataSize > 0) {
    outgoing_byte = *responseData;
    responseData++;
    responseDataSize--;
  }
  else {
    if (hasDataToSend) onResponseSent();
    outgoing_byte = 0xFF;
  }
  outgoing_bit_count = 0;
}

void SPISlavePeripheral::setResponse(uint8_t _data) {
  static uint8_t _response = 0;
  _response = _data;
  setResponse(&_response, 1);
}

void SPISlavePeripheral::setResponse16(uint16_t _data, bool msb) {
  static uint16_t _response = 0;
  if (msb) {
    _response = ((_data << 8) & 0xFF00) | (_data >> 8);
  }
  else {
    _response = _data;
  }
  setResponse((uint8_t*)&_response, 2);
  // printf("setResponse: %d, %d\n", _data, _response);
}

void SPISlavePeripheral::setResponse(uint8_t *_bytes, size_t count) {
  responseData = _bytes;
  responseDataSize = count;
  // if ready, set the next byte to send
  if (outgoing_bit_count == 0) {
    outgoing_bit_count = 0;
    outgoing_byte = *responseData;
    responseData++;
    responseDataSize--;
  }
  hasDataToSend = true;
}

void SPISlavePeripheral::setRequestedDataSize(uint8_t token, size_t _count) {
  currentToken = token;
  requestedDataSize = _count;
  requestedDataIndex = 0;
  delete[] requestedData;
  if (_count > 0) {
    requestedData = new uint8_t[_count];
  }
  else {
    requestedData = nullptr;
  }
}

void SPISlavePeripheral::spiInterrupt(GpioEvent& ev) {
  if (ev.pin_id == cs_pin) {
    if (ev.event == GpioEvent::FALL) onBeginTransaction();
    else if (ev.event == GpioEvent::RISE && insideTransaction) onEndTransaction();
    return;
  }

  if (Gpio::get_pin_value(cs_pin) != 0 || !insideTransaction) return;

  if (ev.pin_id == clk_pin) {
    // == Read ==
    // When CPOL is 0, data must be read when clock RISE
    // When CPOL is 1, data must be read when clock FALL
    if ((ev.event == GpioEvent::RISE && CPOL == 0) || (ev.event == GpioEvent::FALL && CPOL == 1)) {
      const uint8_t currentBit = Gpio::get_pin_value(mosi_pin);
      incoming_byte = (incoming_byte << 1) | currentBit;
      onBitReceived(currentBit);
      if (++incoming_bit_count == 8) {
        onByteReceived(incoming_byte);
        incoming_byte = 0;
        incoming_bit_count = 0;
        incoming_byte_count++;
      }
    }
    // == WRITE ==
    // When CPOL is 0, data must be avaliable when clock FALL
    // When CPOL is 1, data must be avaliable when clock RISE
    else if ((ev.event == GpioEvent::FALL && CPOL == 0) || (ev.event == GpioEvent::RISE && CPOL == 1)) {
      transmitCurrentBit();
    }
  }
}
