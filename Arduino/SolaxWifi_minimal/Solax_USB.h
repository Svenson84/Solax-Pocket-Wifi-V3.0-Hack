#ifndef __SOLAX_USB_H__
#define __SOLAX_USB_H__

#include <Arduino.h>

#define SOLAX_USB_BAURATE           9600  // bps
#define SOLAX_USB_TIMEOUT           1000  // ms
#define SOLAX_USB_INTER_CHAR_DELAY  200   // ms
#define SOLAX_USB_REQUEST_CYCLE     2000  // ms

#define SOLAX_DATA_BYTES              207   // data bytes for RX/TX buffers (+5 bytes will be used for header, length and CRC)

class Solax_USB
{
  private:
    struct SolaxMessageT {
      uint8_t Header[2];
      uint8_t Length;
      uint8_t ControlCode;
      uint8_t FunctionCode;
      uint8_t Data[SOLAX_DATA_BYTES];
      uint8_t DataBytes;
      uint8_t Checksum[2];
    };

    struct SolaxDeviceT {
      uint16_t GridVoltage;     // 0.1 V
      uint16_t OutputCurrent;   // 0.1 A
      uint16_t OutputPower;     // 1 W (Acc. documentation REF{1} it should be 0.1 kW/h, but that is wrong!) 
      uint16_t PV1Voltage;      // 0.1 V
      uint16_t PV2Voltage;      // 0.1 V
      uint16_t PV1Current;      // 0.1 A
      uint16_t PV2Current;      // 0.1 A
      uint16_t PV1Power;        // 1 W
      uint16_t PV2Power;        // 1 W
      uint16_t GridFrequency;   // 0.01 Hz
      uint16_t InverterMode;    // 0: Wait Mode, 1: Normal Mode, 2: Fault Mode, 3: Permanent Fault Mode
      uint32_t EnergyTotal;      // 0.1 kWh
      uint16_t EnergyToday;      // 0.1 kWh
      uint16_t Temperature;     // 1 °C ??
      uint32_t RuntimeTotal;    // 1 h
    };
    
    struct SolaxMessageT RxBuffer;
    uint8_t UartRxPin;
    uint8_t UartTxPin;
    uint8_t RxByteCnt;

    bool GetMsg(uint8_t byte);
    bool ProcessRxMsg(void);
    void RequestInverterData(void);
    void SendTxMsg(struct SolaxMessageT *Buffer);
    uint16_t CalcChecksum(const struct SolaxMessageT Buffer);


  public:
    struct SolaxDeviceT Data;
  
    Solax_USB(uint8_t uart_rx_pin, uint8_t uart_tx_pin);
    void begin();
    void loop(void);
    void ClearRxBuffer(void);
};

#endif /* __SOLAX_USB_H__ */
