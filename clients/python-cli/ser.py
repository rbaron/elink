from typing import Sequence
import serial
import time

from elink_cmd import MAGIC_BYTE

BAUD_RATE = 115200
SIZE = 250 * 128 // 8


def chunkify(data, chunk_size):
    return (
        data[i: i + chunk_size] for i in range(0, len(data), chunk_size)
    )


def send_serial(msgs: Sequence[bytes]):
    ser = serial.Serial("/dev/cu.usbserial-DN06A46M",
                        baudrate=115200, parity=serial.PARITY_NONE)
    for msg in msgs:
        ser.write(MAGIC_BYTE)
        for c in chunkify(msg, 128):
            print('will send', c)
            ser.write(c)
            time.sleep(0.1)
        # Needs ~0.5 on avg, 1s worst case.
        # time.sleep(0.9)
        time.sleep(0.05)
        ss = ser.read_all()
        print(ss)
        print(ss.decode())
    ser.close()
