
#include "SDCard.h"
#include <src/sd/SdInfo.h>

void SDCard::onByteReceived(uint8_t _byte) {
  SPISlavePeripheral::onByteReceived(_byte);
  if (getCurrentToken() != 0xFF || _byte == 0xFF) return;

  // 1 byte (cmd) + 4 byte (arg) + 1 byte (crc)
  const uint8_t cmd = _byte - 0x40;
  switch (cmd) {
    case CMD0:
    case CMD8:
    case CMD55:
    case CMD58:
    case CMD17: //read block
    case CMD24: //write block
    case CMD13:
    case ACMD41:
      setRequestedDataSize(cmd, 5);
      break;
  }
}

void SDCard::onRequestedDataReceived(uint8_t token, uint8_t* _data, size_t count) {
  SPISlavePeripheral::onRequestedDataReceived(token, _data, count);

  uint8_t crc = 0;
  // it should be handled per command, but no other call uses 5 bytes, so it's ok and simpler here
  if (count == 5) {
    currentArg = 0;
    for (int i = 0; i < 4; i++) {
      currentArg <<= 8;
      currentArg |= _data[i];
    }
    crc = _data[4];
  }

  UNUSED(crc);

  // Marlin SD2Card keep the CS LOW for multiple commands, so I need to manually clear the token, to receive next.
  clearCurrentToken();

  // printf("CMD: %d, currentArg: %d, crc: %d, count: %d\n", token, currentArg, crc, count);
  switch (token) {
    case CMD0:
      if (fp) fclose(fp);
      fp = fopen(image_filename.c_str(), "rb+");
      if (fp)
        setResponse(R1_IDLE_STATE);
      else
        setResponse(0);
      break;
    case CMD8:
      if (true/*_type == SD_CARD_TYPE_SD1*/) {
        setResponse((R1_ILLEGAL_COMMAND | R1_IDLE_STATE));
      }
      else {
        memset(buf, 0xAA, 4);
        setResponse(buf, 4);
      }
      break;
    case CMD58:
      buf[0] = R1_READY_STATE;
      memset(buf+1, 0xC0, 3);
      setResponse(buf, 4);
      break;
    case CMD17: //read block
      buf[0] = R1_READY_STATE;
      buf[1] = DATA_START_BLOCK;
      if (true  /*_type != SD_CARD_TYPE_SDHC*/) {
        currentArg >>= 9;
      }
      fseek(fp, 512 * currentArg, SEEK_SET);
      fread(buf + 2, 512, 1, fp);
      buf[512 + 2] = 0; //crc
      setResponse(buf, 512 + 3);
      break;
    case CMD24: //write block
      if (true  /*_type != SD_CARD_TYPE_SDHC*/) {
        currentArg >>= 9;
      }
      setResponse(R1_READY_STATE);
      setRequestedDataSize(DATA_START_BLOCK, 512 + 1 + 2 + 1); //token + ff (from this response) + data + 2 crc
      break;
    case CMD13:
    case CMD55:
    case ACMD41:
      setResponse(R1_READY_STATE);
      break;
    case DATA_START_BLOCK: // CMD24 write block
      fseek(fp, 512 * currentArg, SEEK_SET);
      fwrite(_data + 2, 512, 1, fp);
      fflush(fp);
      buf[0] = DATA_RES_ACCEPTED;
      buf[1] = 0xFF; // ack for finish write
      setResponse(buf, 2);
      break;
  }
}
