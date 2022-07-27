import asyncio
from time import time
from typing import Optional, Sequence
from bleak import BleakClient, BleakError, BleakScanner
from bleak.backends.scanner import AdvertisementData
from bleak.backends.device import BLEDevice

from elink_cmd import MAGIC_BYTE


SERVICE_UUID = '13187B10-EBA9-A3BA-044E-83D3217D9A38'
TX_CHARACTERISTIC_UUID = '4B646063-6264-F3A7-8941-E65356EA82FE'

SCAN_TIMEOUT_S = 30

WAIT_AFTER_DATA_SENT_S = 3
# WAIT_AFTER_DATA_SENT_S = 10


async def scan(name: Optional[str], logger, timeout: int):
    autodiscover = (not name)
    if autodiscover:
        logger.info('⏳ Trying to auto-discover..')
    else:
        logger.info(f'⏳ Looking for a BLE device named {name}...')

    def filter_fn(device: BLEDevice, adv_data: AdvertisementData):
        if autodiscover:
            return SERVICE_UUID in adv_data.service_uuids
        else:
            return device.name == name

    device = await BleakScanner.find_device_by_filter(
        filter_fn, timeout=timeout,
    )
    if device is None:
        raise RuntimeError(
            'Unable to find device, make sure it is turned on and in range')
    logger.info(f'✅ Got it. Address: {device}')
    return device


def chunkify(data, chunk_size):
    return (
        data[i: i + chunk_size] for i in range(0, len(data), chunk_size)
    )


# async def run_ble(data, devicename, logger):
async def send_ble(device, msgs: Sequence[bytes], logger):
    # address = await scan(devicename, SCAN_TIMEOUT_S, logger)
    logger.info(f'⏳ Connecting to {device}...')
    async with BleakClient(device, timeout=30.0) as client:
        logger.info(
            f'✅ Connected: {client.is_connected}; MTU: {client.mtu_size}')
        chunk_size = client.mtu_size - 3 - 1
        for msg in msgs:
            msg = MAGIC_BYTE + msg
            logger.info(
                f'⏳ Sending {len(msg)} bytes of data in chunks of {chunk_size} bytes...')
            for i, chunk in enumerate(chunkify(msg, chunk_size)):
                await client.write_gatt_char(TX_CHARACTERISTIC_UUID, chunk)
                print(f'Sent {len(chunk)} bytes')
            await asyncio.sleep(0.2)

        logger.info(f'✅ Done.')
        await asyncio.sleep(WAIT_AFTER_DATA_SENT_S)
