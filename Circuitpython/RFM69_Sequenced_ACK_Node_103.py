import board
import busio
from math import atan, atan2, cos, pi, sin
from digitalio import DigitalInOut, Direction, Pull
from time import sleep

DEBUG = True

PIN_ONBOARD_LED = board.D13
PIN_PACKET_LED = board.D6                                                                                                                                                                                                                                                                                                                                              

SPI_SCK = board.SCK
SPI_MISO = board.MISO
SPI_MOSI = board.MOSI

RFM69_CS = DigitalInOut(board.D4)
RFM69_RST = DigitalInOut(board.D5)
RFM69_NETWORK_NODE = 103

#   Frequency of the radio in Mhz. Must match your
#       module! Can be a value like 915.0, 433.0, etc.
RFM69_RADIO_FREQ_MHZ = 915.0

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

	while n > 0:
		g.append(n & 0xFF)
		n = n >> 8

	t = ""

	for index in range(len(g)):
		t = chr(g[index]) + t

	while len(t) < length:
		t = chr(0) + t

	return t

def unpack(st):
	n = 0

	for index in range(len(st)):
		n = n + (ord(st[index]) << (8 * (len(st) - index - 1)))

	return n

#   Initialize the onboard LED
heartBeatLED = DigitalInOut(PIN_ONBOARD_LED)
heartBeatLED.direction = Direction.OUTPUT

packetReceivedLED = DigitalInOut(PIN_PACKET_LED)
packetReceivedLED.direction = Direction.OUTPUT

#	Initialize the I2C bus
i2c = busio.I2C(board.SCL, board.SDA)

#   Initialize the SPI bus
spi = busio.SPI(SPI_SCK, MOSI=SPI_MOSI, MISO=SPI_MISO)

print("This is node #{0}".format(RFM69_NETWORK_NODE))
print()

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
        toNodeAddress = 102

        #   Pack the packet
        packedSequence = pack(packetSentCount, 4)
        packedFromNode = pack(RFM69_NETWORK_NODE, 2)
        packedToNode = pack(toNodeAddress, 2)
        packedTotalPackets = pack(25, 1)
        packedSubPacketNumber = pack(12, 1)
        payload = "Hello node {0}".format(103)
        packedPacketLength = pack(len(packedSequence) + len(packedFromNode) + len(packedToNode) + len(packedTotalPackets) + len(packedSubPacketNumber) + len(payload) + 1, 1)

        packetHeader = packedSequence + packedFromNode + packedToNode + packedPacketLength + packedTotalPackets + packedSubPacketNumber

        outPacket = packetHeader + payload
       
        print("Sending {0:4d} '{1}' message!".format(packetSentCount, payload))
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
        # Received a packet!
        receivedPacket = True
        packetReceivedCount += 1
        packetReceivedLED.value = True
        # Print out the raw bytes of the packet:

        if DEBUG:
            print("packet[0:4] = '{0}', packet[4:6] = '{1}', packet[6:] = '{2}'".format(packet[0:4], packet[4:6], packet[6:]))

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

            #   Unpack the packet
            sequenceNumberIn = unpack(outPacket[0:4])
            fromNodeAddress = unpack(outPacket[4:6])
            toNodeAddress = unpack(outPacket[6:8])
            packetLenthIn = unpack(outPacket[8:9])
            totalPacketsIn = unpack(outPacket[9:10])
            subPacketNumberIn = unpack(outPacket[10:11])
            payloadIn = outPacket[11:]

            #
            #   Add packet validation here
            #

            #   Decode the payload
            payload_text = str(payloadIn, 'ASCII')
            print("Received (ASCII): '{0}'".format(payloadIn_text))

            sleep(0.2)

            #   ACK the packet
            ackPacket = pack(packetNumberIn, 4) + pack(RFM69_NETWORK_NODE, 2) + "ACK"
            rfm69.send(bytes(ackPacket, "utf-8"))

    sleep(0.2)
