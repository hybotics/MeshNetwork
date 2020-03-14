import board
import busio
from math import atan, atan2, cos, pi, sin
from digitalio import DigitalInOut, Direction, Pull
from time import sleep

DEBUG = True

PIN_ONBOARD_LED = board.D13
PIN_PACKET_LED = board.D2                                                                                                                                                                                                                                                                                                                                              

SPI_SCK = board.SCK
SPI_MISO = board.MISO
SPI_MOSI = board.MOSI

BLUEFRUIT_CS = DigitalInOut(board.D12)
BLUEFRUIT_IRQ = DigitalInOut(board.D7)
BLUEFRUIT_RST = DigitalInOut(board.D9)

BLUEFRUIT_DEV_NAME = "FrankenCircuit"
BLUEFRUIT_BEACON_NAME = "Frank0.0"

SCRIPT_VERSION = 0.0

RFM69_CS = DigitalInOut(board.D10)
RFM69_RST = DigitalInOut(board.D11)
RFM69_NETWORK_NODE = 102

#   Frequency of the radio in Mhz. Must match your
#       module! Can be a value like 915.0, 433.0, etc.
RFM69_RADIO_FREQ_MHZ = 915.0

from adafruit_bluefruitspi import BluefruitSPI

import adafruit_fxos8700
import adafruit_fxas21002c

import adafruit_bus_device
import adafruit_register
import adafruit_lsm303_accel
import adafruit_lsm303dlh_mag

import adafruit_rfm69

def blinkLED(led, wait=0.2, cycles=1):
	for cy in range(cycles):
		led.value = True
		sleep(wait)
		led.value = False
		sleep(wait)

def pack(number, length=4):
    n = number
    g = []

    #   Dissect the number into bytes (8-bits)
    while n > 0:
        g.append(n & 0xFF)
        n = n >> 8

    t = ""

    #   Pack it
    for index in range(len(g)):
        t = chr(g[index]) + t

    #   Left pad the packed number
    while len(t) < length:
        t = chr(0) + t

    return t

def unpack(st):
    n = 0

    for index in range(len(st)):
        n = n + (ord(st[index]) << (8 * (len(st) - index - 1)))

    return n

#	Convert anglular data to degrees
def angleToDegrees(angle, p):
	return angle * 180 / p

def simpleOrientation(acc_x, acc_y, acc_z, mag_x, mag_y, mag_z, p):
#	float const PI_F = 3.14159265F;

#	roll: Rotation around the X-axis. -180 <= roll <= 180
#	a positive roll angle is defined to be a clockwise rotation about the positive X-axis
#
#							  y
#				roll = atan2(---)
#							  z
#
#	where:  y, z are returned value from accelerometer sensor
	roll = atan2(acc_y, acc_z);

#	pitch: Rotation around the Y-axis. -180 <= roll <= 180
#	a positive pitch angle is defined to be a clockwise rotation about the positive Y-axis
#
#							-x
#	pitch = atan(----------------------------------)
#					y * sin(roll) + z * cos(roll)
#
#	where:  x, y, z are returned value from accelerometer sensor
	if (acc_y * sin(roll) + acc_z * cos(roll) == 0):
		#pitch = (acc_x > 0 ? (pi / 2) : (-pi / 2));
		if (acc_x > 0):
			pitch = (p / 2)
		else:
			pitch = (-p / 2)
	else:
		pitch = atan(-acc_x / (acc_y * sin(roll) + acc_z * cos(roll)))

#	heading: Rotation around the Z-axis. -180 <= roll <= 180
#	a positive heading angle is defined to be a clockwise rotation about the positive Z-axis
#
#										z * sin(roll) - y * cos(roll)
#	heading = atan2(--------------------------------------------------------------------------)
#					x * cos(pitch) + y * sin(pitch) * sin(roll) + z * sin(pitch) * cos(roll))
#
#	where:  x, y, z are returned value from magnetometer sensor
	heading = atan2(mag_z * sin(roll) - mag_y * cos(roll),
									  mag_x * cos(pitch) + mag_y * sin(pitch) * sin(roll) +
									  mag_z * sin(pitch) * cos(roll))

#	Convert angular data to degree
	roll = angleToDegrees(roll, pi)
	pitch = angleToDegrees(pitch, pi)
	heading = angleToDegrees(heading, pi) + 180

 	return roll, pitch, heading

#   Initialize the onboard LED
heartBeatLED = DigitalInOut(PIN_ONBOARD_LED)
heartBeatLED.direction = Direction.OUTPUT

packetReceivedLED = DigitalInOut(PIN_PACKET_LED)
packetReceivedLED.direction = Direction.OUTPUT

#	Initialize the I2C bus
i2c = busio.I2C(board.SCL, board.SDA)

#   Initialize the SPI bus
spi = busio.SPI(SPI_SCK, MOSI=SPI_MOSI, MISO=SPI_MISO)

print()
print("{0} v{1:1.1f}".format(BLUEFRUIT_DEV_NAME, SCRIPT_VERSION))

#   Initialize the Bluefruit LE SPI Friend and perform a factory reset
bluefruit = BluefruitSPI(spi, cs=BLUEFRUIT_CS, irq=BLUEFRUIT_IRQ, reset=BLUEFRUIT_RST)

print()
print("Initializing Bluetooth")
bluefruit.init()
bluefruit.command_check_OK(b'AT+FACTORYRESET', delay=1)

#   Print the response to 'ATI' (info request) as a string
print(str(bluefruit.command_check_OK(b'ATI'), 'utf-8'))

#   Change advertised name
bluefruit.command_check_OK(b'AT+GAPDEVNAME=' + BLUEFRUIT_DEV_NAME)

#   Set the name of the beacon
##BLUEFRUIT_BEACON_NAME = "FrankBLE"
##bluefruit.command_check_OK(b'AT+BLEURIBEACON=' BLUEFRUIT_BEACON_NAME) 

#   Initialze RFM69 radio
print("Initializing the RFM69 radio")
rfm69 = adafruit_rfm69.RFM69(spi, RFM69_CS, RFM69_RST, RFM69_RADIO_FREQ_MHZ)

# Optionally set an encryption key (16 byte AES key). MUST match both
# on the transmitter and receiver (or be set to None to disable/the default).
rfm69.encryption_key = b'\x01\x02\x03\x04\x05\x06\x07\x08\x01\x02\x03\x04\x05\x06\x07\x08'

rfm69Celsius = rfm69.temperature
rfm69Fahrenheit = round(rfm69Celsius * 1.8 + 32, 1)

#   Print out some RFM69 chip state:
print("RFM69 Radio Data")
print('    Temperature:         {0}°F ({1}°C)'.format(rfm69Fahrenheit, rfm69Celsius))
print('    Frequency:           {0} MHz'.format(round(rfm69.frequency_mhz, 0)))
print('    Bit rate:            {0} kbit/s'.format(rfm69.bitrate / 1000))
print('    Frequency deviation: {0} kHz'.format(rfm69.frequency_deviation / 1000))

#   Initialize the LSM303
lsmMag = adafruit_lsm303dlh_mag.LSM303DLH_Mag(i2c)
lsmAcc = adafruit_lsm303_accel.LSM303_Accel(i2c)

#   Initialize the NXP IMU
nxpAcc = adafruit_fxos8700.FXOS8700(i2c)
nxpGyro = adafruit_fxas21002c.FXAS21002C(i2c)

loopCount = 0
receivedPacket = True
packetReceivedCount = 0
packetSentCount = 0
resendMessage = False
sequenceNumber = 0
ackPacketsReceived = 0
acknowledged = True

print()

while True:
    blinkLED(heartBeatLED)
    loopCount += 1

    if DEBUG:
        print("Loop #{0:6d}".format(loopCount))
        print()

    #   Put RFM69 radio stuff here
    if acknowledged:
        packetSentCount += 1

        packedSequence = pack(packetSentCount)
        outPacket = packedSequence + "Hello World"
        print('Sending {0:4d} "Hello World" message!'.format(packetSentCount))
        ##("Sending" if receivedPacket else "ReSending"), packetSentCount))
        #rfm69.send(bytes("Hello World #{0}\r\n".format(packetSentCount), "utf-8"))
        rfm69.send(bytes(outPacket, "utf-8"))
        acknowledged = False

    resendMessage = False
    receivedPacket = False
    print()

    if not receivedPacket:
        sleep(0.2)

    print('Waiting for packets...')

    if resendMessage:
        windowSize = 2.0
    else:
        windowSize = 1.5

    packet = rfm69.receive(timeout=windowSize)

    if packet is None:
        # Packet has not been received
        resendMessage = True
        receivedPacket = False
        packetReceivedLED.value = False
        print('Received nothing! Listening again...')
        print()
    else:
        # Received a new packet!
        receivedPacket = True
        packetReceivedCount += 1
        packetReceivedLED.value = True
        # Print out the raw bytes of the packet:

        packetNumberIn = unpack(packet[0:4])
        fromNodeAddress = unpack(packet[4:6])
        packet = packet[6:]

        if packet.find("ACK") and packetNumberIn == packetSentCount:
            #   ACK packet
            acknowledged = True
            print("Received ACK of packet {0} from node {1}".format(packetNumberIn, fromNodeAddress))
        else:
            #   New packet
            print("Received new packet #{0} (raw bytes): '{1}', from node {2}".format(packetNumberIn, packet, fromNodeAddress))
            # And decode to ASCII text and print it too.  Note that you always
            # receive raw bytes and need to convert to a text format like ASCII
            # if you intend to do string processing on your data.  Make sure the
            # sending side is sending ASCII data before you try to decode!

            #
            #   Add packet validation here
            #

            packet_text = str(packet, 'ASCII')
            print("Received (ASCII): '{0}'".format(packet_text))

            sleep(0.2)

            #   ACK the packet
            ackPacket = pack(packetNumberIn) + pack(RFM69_NETWORK_NODE, 2) + "ACK"
            rfm69.send(bytes(ackPacket, "utf-8"))
    
    lsm_acc_x, lsm_acc_y, lsm_acc_z = lsmAcc.acceleration
    lsm_mag_x, lsm_mag_y, lsm_mag_z = lsmMag.magnetic

    if DEBUG:
        print("LSM Acc (m/s^2):      x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(lsm_acc_x, lsm_acc_y, lsm_acc_z))
        print("LSM Mag (uTeslas):    x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(lsm_mag_x, lsm_mag_y, lsm_mag_z))
        print()

    #	Read data from the NXP IMU, accelerometer, magnetometer, and gyroscope
    nxp_acc_x, nxp_acc_y, nxp_acc_z = nxpAcc.accelerometer
    nxp_mag_x, nxp_mag_y, nxp_mag_z = nxpAcc.magnetometer
    nxp_gyro_x, nxp_gyro_y, nxp_gyro_z = nxpGyro.gyroscope

    #	Calculate simple roll, pitch, and heading from accelerometer and magnetometer readings
    roll, pitch, heading = simpleOrientation(nxp_acc_x, nxp_acc_y, nxp_acc_z, nxp_mag_x, nxp_mag_y, nxp_mag_z, pi)

    if DEBUG:
        print("NXP Acc (m/s^2):      x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(nxp_acc_x, nxp_acc_y, nxp_acc_z))
        #print("NXP Mag (gauss):      x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(nxp_mag_x, nxp_mag_y, nxp_mag_z))
        print("NXP Mag (uTeslas):    x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(nxp_mag_x / 10.0, nxp_mag_y / 10.0, nxp_mag_z / 10.0))
        print("NXP Gyro (radians/s): x = {0:11.5f},    y = {1:11.5f},   z = {2:11.5f}".format(nxp_gyro_x, nxp_gyro_y, nxp_gyro_z))
        print()
        print("Roll = {0:5.2f}, Pitch = {1:5.2f}, Heading = {2:5.2f}".format(roll, pitch, heading))
        print()

    #sleep(0.2)
