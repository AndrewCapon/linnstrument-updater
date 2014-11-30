/*
 Copyright 2014 Roger Linn Design (www.rogerlinndesign.com)
 
 Written by Geert Bevin (http://gbevin.com).
 
 This work is licensed under the Creative Commons Attribution-ShareAlike 3.0 Unported License.
 To view a copy of this license, visit http://creativecommons.org/licenses/by-sa/3.0/
 or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
*/
#include "LinnStrumentSerialMac.h"

#include "UpdaterApplication.h"

#include "serial/serial.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#include <AvailabilityMacros.h>
#include <sys/param.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#include <errno.h>

bool LinnStrumentSerial::readSettings()
{
    if (!isDetected()) return false;
    
    settings.clear();
    
    try {
        serial::Serial linnSerial(getFullLinnStrumentDevice().toRawUTF8(), 115200, serial::Timeout::simpleTimeout(5000));
        
        if (!linnSerial.isOpen()) {
            std::cerr << "Wasn't able to open serial device " << getFullLinnStrumentDevice() << " with baud rate 115200" << std::endl;
            return false;
        }
        
        MessageManager::getInstance()->runDispatchLoopUntil(1500);
        
        if (linnSerial.write("5, 4, 3, 2, 1 ...\n") != 18) {
            std::cerr << "Couldn't write the complete handshake message to serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        std::string linnGoCode = linnSerial.readline();
        if (linnGoCode != "LinnStruments are go!\n") {
            std::cerr << "Didn't receive the correct go code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        if (linnSerial.write("s") != 1) {
            std::cerr << "Couldn't to give the read settings command to serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        std::string ackCode = linnSerial.readline();
        if (ackCode != "ACK\n") {
            std::cerr << "Didn't receive the ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        uint8_t sizeBuffer[4];
        if (linnSerial.read(sizeBuffer, 4) != 4) {
            std::cerr << "Couldn't retrieve the size of the settings from device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        int32_t settingsSize;
        std::memcpy(&settingsSize, sizeBuffer, sizeof(int32_t));
        
        if (linnSerial.read(settings, settingsSize) != settingsSize) {
            std::cerr << "Couldn't retrieve the settings from device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        ackCode = linnSerial.readline();
        if (ackCode != "ACK\n") {
            std::cerr << "Didn't receive read settings finish ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
    }
    catch (serial::SerialException e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
    
    return true;
}

bool LinnStrumentSerial::restoreSettings()
{
    if (!isDetected()) return false;
    
    try {
        serial::Serial linnSerial(getFullLinnStrumentDevice().toRawUTF8(), 115200, serial::Timeout::simpleTimeout(5000));
        
        if (!linnSerial.isOpen()) {
            std::cerr << "Wasn't able to open serial device " << getFullLinnStrumentDevice() << " with baud rate 115200" << std::endl;
            return false;
        }
        
        MessageManager::getInstance()->runDispatchLoopUntil(1500);
        
        if (linnSerial.write("5, 4, 3, 2, 1 ...\n") != 18) {
            std::cerr << "Couldn't write the complete handshake message to serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        std::string linnGoCode = linnSerial.readline();
        if (linnGoCode != "LinnStruments are go!\n") {
            std::cerr << "Didn't receive the correct go code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        if (linnSerial.write("r") != 1) {
            std::cerr << "Couldn't to give the restore settings command to serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        MessageManager::getInstance()->runDispatchLoopUntil(10);
        std::string ackCode = linnSerial.readline();
        if (ackCode != "ACK\n") {
            std::cerr << "Didn't receive the restore settings ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        int32_t settingsSize = settings.size();
        uint8_t sizeBuffer[4];
        std::memcpy(sizeBuffer, &settingsSize, sizeof(int32_t));
        
        if (linnSerial.write(sizeBuffer, 4) != 4) {
            std::cerr << "Couldn't write the size of the settings to device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        uint8_t settingsBuffer[settingsSize];
        copy(settings.begin(), settings.end(), settingsBuffer);
        
        ackCode = linnSerial.readline();
        if (ackCode != "ACK\n") {
            std::cerr << "Didn't receive restore settings progress ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }

        uint8_t batchsize = 32;
        uint8_t* source = settingsBuffer;
        int i = 0;
        while (i+batchsize < settingsSize) {
            MessageManager::getInstance()->runDispatchLoopUntil(10);
            if (linnSerial.write(source+i, batchsize) != batchsize) {
                std::cerr << "Couldn't write the settings to device " << getFullLinnStrumentDevice() << std::endl;
                return false;
            }
            i += batchsize;
            
            ackCode = linnSerial.readline();
            if (ackCode != "ACK\n") {
                std::cerr << "Didn't receive restore settings progress ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
                return false;
            }
        }
        
        int remaining = settingsSize - i;
        if (remaining > 0) {
            MessageManager::getInstance()->runDispatchLoopUntil(10);
            if (linnSerial.write(source+i, remaining) != remaining) {
                std::cerr << "Couldn't write the settings to device " << getFullLinnStrumentDevice() << std::endl;
                return false;
            }
        }
        
        ackCode = linnSerial.readline();
        if (ackCode != "ACK\n") {
            std::cerr << "Didn't receive restore settings finish ACK code from serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
        
        MessageManager::getInstance()->runDispatchLoopUntil(100);

        if (linnSerial.write("d") != 1) {
            std::cerr << "Couldn't to give the done command to serial device " << getFullLinnStrumentDevice() << std::endl;
            return false;
        }
    }
    catch (serial::SerialException e) {
        std::cerr << e.what() << std::endl;
        return false;
    }
    
    return true;
}