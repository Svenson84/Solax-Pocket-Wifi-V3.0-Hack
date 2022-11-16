#include <Arduino.h>
#include "Solax_USB.h"

Solax_USB::Solax_USB(uint8_t uart_rx_pin, uint8_t uart_tx_pin)
{
  UartRxPin = uart_rx_pin;
  UartTxPin = uart_tx_pin;

  RxByteCnt = 0;
  RxBuffer = {0};
  Data = {0};
}

void Solax_USB::begin(void) {
  // Configure Serial Port (UART1) for communication with Solax X1 Mini via USB (UART)
  Serial1.setTimeout(SOLAX_USB_TIMEOUT);  
  Serial1.begin(SOLAX_USB_BAURATE, SERIAL_8N1, UartRxPin, UartTxPin); // SERIAL_8N1 (default) --> 8 data bits, No parity, 1 stop bit
}

void Solax_USB::loop(void) {
  const uint32_t now = millis();
  static uint32_t LastRequest = 0;
  static uint32_t LastRxByte = 0;
  
  if((now - LastRxByte) > SOLAX_USB_INTER_CHAR_DELAY) {
    //Serial.println("[USB] Clear Buffer");
    ClearRxBuffer();
  }

  // Send Request to Inverter (Tx Message)
  if((now - LastRequest) > SOLAX_USB_REQUEST_CYCLE) {
    Serial.println("[USB] Request Inverter Data");
    RequestInverterData();
    LastRequest = now;
  }

  delay(5);
  
  // Read Response from Inverter (Rx Message)
  while (Serial1.available()) {
    uint8_t byte;
    byte = Serial1.read();
    LastRxByte = millis();
    if (GetMsg(byte)) {
      Serial.println("[USB] Get message complete");
      if(ProcessRxMsg()) {
        Serial.println("[USB] Message processed");
      }
    } 
  }
}


void Solax_USB::ClearRxBuffer(void) {
  RxByteCnt = 0;
}


// Function: GetMsg()
//  Stores received byte from USB(UART) in the RX Buffer.
// Parameter:
//  uint8_t byte: Provide new byte from USB(UART) interface.
// Return:
//  bool: TRUE if USB(UART) message from header to checksum was completely received.
//        FALSE if USB(UART) message is still incomplete or new byte was invalid.
bool Solax_USB::GetMsg(uint8_t byte) {
  RxByteCnt++;
  
  // Byte 1: Header MSB (0xAA)
  if(RxByteCnt == 1) {
    if(byte == 0xAA) {
      RxBuffer.Header[1] = byte;
    }
    else {
      ClearRxBuffer();
    }
    return(false);
  }
  
  // Byte 2: Header LSB (0x55)
  if(RxByteCnt == 2) {
    if(byte == 0x55) {
      RxBuffer.Header[0] = byte;
    }
    else {
      ClearRxBuffer();
    }
    return(false);
  }
  
  // Byte 3: Length
  if(RxByteCnt == 3) {
    RxBuffer.Length = byte;
    RxBuffer.DataBytes = RxBuffer.Length - 7;
    return(false);
  }

  // Byte 4: Control Byte
  if(RxByteCnt == 4) {
    RxBuffer.ControlCode = byte;
    return(false);
  }
  
  // Byte 5: Function Code
  if(RxByteCnt == 5) {
    RxBuffer.FunctionCode = byte;
    return(false);
  }
  
  // Byte 6...(6+N): Data Bytes
  if(RxByteCnt >= 6 && RxByteCnt < (6 + RxBuffer.DataBytes)) {
    RxBuffer.Data[RxByteCnt - 6] = byte;
    return(false);
  }
  
  // Byte 7+N: Checksum MSB
  if(RxByteCnt == (6 + RxBuffer.DataBytes)) {
    RxBuffer.Checksum[1] = byte;
    return(false);
  }

  // Byte 8+N: Checksum LSB
  if(RxByteCnt == (7 + RxBuffer.DataBytes)) {
    // Message completely received!
    RxBuffer.Checksum[0] = byte; 

    // Check Checksum
    uint16_t ReceivedChecksum = (uint16_t) RxBuffer.Checksum[0] << 8 | (uint16_t) RxBuffer.Checksum[1];
    uint16_t CalculatedChecksum = CalcChecksum(RxBuffer);

    if (ReceivedChecksum != CalculatedChecksum) {
      Serial.println("[SolaxGetMsg] CRC mismatch");
      return(false);
    }
  }
  else {
    Serial.println("[SolaxGetMsg] Byte read invalid");
    ClearRxBuffer();
    return(false);
  }

  return(true);
}

// Function: ProcessRxMsg()
//  Processes a completely received message.
// Parameter:
//  const struct SolaxMessageT: Provide new message to process.
// Return:
//  bool: TRUE if message was successful processed.
//        FALSE if an error occured.
bool Solax_USB::ProcessRxMsg(void) {
  bool ReturnValue = false;
  
  if(RxBuffer.ControlCode == 0x01 && RxBuffer.FunctionCode == 0x8c) {
    // total bytes: 207 bytes
    // dataarray:
    //       2 bytes preamble (0xaa 0x55), 
    //       1 byte data length (0xcf), 
    //       1 byte control code (0x01), 
    //       1 byte function code (0x8c), 
    //       200 bytes payload,
    //       2 bytes crc
    
    Data.GridVoltage = (RxBuffer.Data[1]<<8) + (RxBuffer.Data[0]);
    Data.OutputCurrent = (RxBuffer.Data[3]<<8) + (RxBuffer.Data[2]);
    Data.OutputPower = (RxBuffer.Data[5]<<8) + (RxBuffer.Data[4]);
    Data.PV1Voltage = (RxBuffer.Data[7]<<8) + (RxBuffer.Data[6]);
    Data.PV2Voltage = (RxBuffer.Data[9]<<8) + (RxBuffer.Data[8]);
    Data.PV1Current = (RxBuffer.Data[11]<<8) + (RxBuffer.Data[10]);
    Data.PV2Current = (RxBuffer.Data[13]<<8) + (RxBuffer.Data[12]);
    Data.PV1Power = (RxBuffer.Data[15]<<8) + (RxBuffer.Data[14]);
    Data.PV2Power = (RxBuffer.Data[17]<<8) + (RxBuffer.Data[16]);
    Data.GridFrequency = (RxBuffer.Data[19]<<8) + (RxBuffer.Data[18]);
    Data.InverterMode = RxBuffer.Data[20];
    Data.EnergyTotal = (RxBuffer.Data[25]<<24) + (RxBuffer.Data[24]<<16) + (RxBuffer.Data[23]<<8) + (RxBuffer.Data[22]);
    Data.EnergyToday = (RxBuffer.Data[27]<<8) + (RxBuffer.Data[26]);
    // bytes 27..77 not used
    Data.Temperature = (RxBuffer.Data[79]<<8) + (RxBuffer.Data[78]);
    // bytes 80..81 not used
    Data.RuntimeTotal = (RxBuffer.Data[85]<<24) + (RxBuffer.Data[84]<<16) + (RxBuffer.Data[83]<<8) + (RxBuffer.Data[82]);
    // bytes 86..199 unkown

    Serial.print("GridVoltage: ");
    Serial.print((Data.GridVoltage / 10.0), 1);
    Serial.println(" V");

    Serial.print("OutputCurrent: ");
    Serial.print((Data.OutputCurrent / 10.0), 1);
    Serial.println(" A");

    Serial.print("OutputPower: ");
    Serial.print(Data.OutputPower, DEC);
    Serial.println(" W");

    Serial.print("PV1Voltage: ");
    Serial.print((Data.PV1Voltage / 10.0), 1);
    Serial.println(" V");

    Serial.print("PV2Voltage: ");
    Serial.print((Data.PV2Voltage / 10.0), 1);
    Serial.println(" V");
    
    Serial.print("PV1Current: ");
    Serial.print((Data.PV1Current / 10.0), 1);
    Serial.println(" A");

    Serial.print("PV2Current: ");
    Serial.print((Data.PV2Current / 10.0), 1);
    Serial.println(" A");

    Serial.print("PV1Power: ");
    Serial.print(Data.PV1Power, DEC);
    Serial.println(" W");

    Serial.print("PV2Power: ");
    Serial.print(Data.PV2Power, DEC);
    Serial.println(" W");

    Serial.print("GridFrequency: ");
    Serial.print((Data.GridFrequency / 100.0), 2);
    Serial.println(" Hz");
    
    Serial.print("InverterMode: ");
    Serial.println(Data.InverterMode, DEC);
    
    Serial.print("EnergyTotal: ");
    Serial.print((Data.EnergyTotal / 10.0), 1);
    Serial.println(" kWh");
    
    Serial.print("EnergyToday: ");
    Serial.print((Data.EnergyToday / 10.0), 1);
    Serial.println(" kWh");
    
    Serial.print("Temperature: ");
    Serial.print(Data.Temperature, DEC);
    Serial.println(" C");
    
    Serial.print("RuntimeTotal: ");
    Serial.print(Data.RuntimeTotal, DEC);
    Serial.println(" h");

    ReturnValue = true;
  }
  return(ReturnValue);
}


void Solax_USB::RequestInverterData(void) {
  static SolaxMessageT TxMsg;

  TxMsg.ControlCode = 0x01;
  TxMsg.FunctionCode = 0x0C;
  TxMsg.DataBytes = 0; 

  SendTxMsg(&TxMsg);
}


void Solax_USB::SendTxMsg(SolaxMessageT *Buffer) {
  uint16_t Checksum = 0;
  
  Buffer->Header[0] = 0xAA;
  Buffer->Header[1] = 0x55;

  Buffer->Length = Buffer->DataBytes + 7; // data bytes + 7 bytes (2 bytes header, 1 byte lenght, 1 byte control code, 1 byte function code, 2 bytes CRC)

  Checksum = CalcChecksum((const SolaxMessageT) *Buffer);
  Buffer->Checksum[1] = Checksum >> 8;
  Buffer->Checksum[0] = Checksum;

  // Output to Debug Interface
  Serial.print("[USB] SendTxMsg: ");
  Serial.print(Buffer->Header[0], HEX);
  Serial.print(" ");
  Serial.print(Buffer->Header[1], HEX);
  Serial.print(" ");
  Serial.print(Buffer->Length, HEX);
  Serial.print(" ");
  Serial.print(Buffer->ControlCode, HEX);
  Serial.print(" ");
  Serial.print(Buffer->FunctionCode, HEX);
  Serial.print(" ");
  Serial.print(Buffer->Checksum[0], HEX);
  Serial.print(" ");
  Serial.println(Buffer->Checksum[1], HEX);

  // Output to Inverter
  Serial1.write((const uint8_t *) Buffer, (Buffer->Length - 2));
  Serial1.write(Buffer->Checksum[0]);
  Serial1.write(Buffer->Checksum[1]);
  Serial1.flush();
}


// Function: CalcChecksum()
//  Calculates the checksum of all bytes in a buffer.
//  Checksum = Header + Length + Control Code + Function Code + Data0 + .. + Data (N-1)
// Parameter:
//  struct SolaxMessageT *: Provides the header, control code, function code and data bytes as input for checksum calculation.
// Return:
//  uint16_t: Calculated checksum
uint16_t Solax_USB::CalcChecksum(const struct SolaxMessageT Buffer) {
  uint8_t i;
  uint16_t Checksum = 0;

  Checksum += Buffer.Header[0];
  Checksum += Buffer.Header[1];
  Checksum += Buffer.Length;
  Checksum += Buffer.ControlCode;
  Checksum += Buffer.FunctionCode;

  for (i = 0; i <= (Buffer.DataBytes); i++) {
    Checksum += Buffer.Data[i];
  }

  return(Checksum);
}
