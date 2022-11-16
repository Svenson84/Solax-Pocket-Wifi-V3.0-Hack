import keyboard
import serial
import time
from datetime import datetime

print("-------------------------------------")
print("Sniffing Serial Communication on COM6")
print("-------------------------------------")
now = datetime.now()
OutputFile = now.strftime("%Y-%m-%d_%H%M%S.log")
print("Using Logfile: " + OutputFile)
logfile = open(OutputFile, "w")

TimeoutTime = 0.200
LastTimestamp = 0
DataBuffer = []
HexString = ''
ser = serial.Serial('COM6', 9600)

if not ser.is_open:
    print("Open Port!")
    ser.open()
    ser.flushInput()

while 1:
    LastChar = ser.read(1)
    ActualTimestamp = datetime.timestamp(datetime.now())
    if( ( ActualTimestamp - LastTimestamp ) > TimeoutTime and HexString != ''):
        txt = "[{:.3f}]: {}".format(LastTimestamp, HexString)
        print(txt)
        logfile.write(txt+"\n")
        HexString = ''
    
    HexString = HexString + LastChar.hex(' ')
    LastTimestamp = ActualTimestamp
    
    try:
        if keyboard.is_pressed('q'):
            print("Stop")
            break;
    except:
        print("Press 'q' to stop!")

logfile.close()