import argparse
import asyncio
import logging
import sys
import os
import numpy as np
from ble import scan, send_ble
from elink_cmd import cmd_char, cmd_img, cmd_ping, cmd_sleep, cmd_str

from img import read_img
from ser import send_serial
from txt import Font

EPD_WIDTH = 250
EPD_HEIGHT = 128


def img(args, logger):
    filename = args.filename
    if not os.path.exists(filename):
        logger.info('🛑 File not found. Exiting.')
        return

    bin_img = read_img(args.filename, EPD_HEIGHT,
                       logger, args.binarization_algo, args.show_preview, args.invert)
    if bin_img is None:
        logger.info(f'🛑 No image generated. Exiting.')
        return

    logger.info(f'✅ Read image: {bin_img.shape} (h, w) pixels')

    data = np.packbits(bin_img)
    return asyncio.run(args.send_message(cmd_img(args.to, bytes(data))))


def ping(args, logger):
    return asyncio.run(args.send_message(cmd_ping(args.to)))


def char(args, logger):
    return asyncio.run(args.send_message(cmd_char(args.font, args.string, args.invert)))


def string(args, logger):
    return asyncio.run(args.send_message(
        cmd_str(args.to, args.font, args.y, args.x, args.string, args.invert)))


def sleep(args, logger):
    return asyncio.run(args.send_message(cmd_sleep(args.to, args.wait_ms)))


def parse_args():
    args = argparse.ArgumentParser(
        description='Sends text and image data to the elink array of e-paper displays.')
    args.add_argument('--log-level', type=str,
                      choices=['debug', 'info', 'warn', 'error'], default='info')
    args.add_argument('--transport', type=str,
                      choices=['uart', 'ble'], default='uart')
    args.add_argument('--to', type=int, default=0)
    subparsers = args.add_subparsers()

    img_parser = subparsers.add_parser('img')
    img_parser.add_argument('filename', type=str)
    img_parser.add_argument('--binarization-algo', type=str,
                            choices=['mean-threshold', 'floyd-steinberg'], default='floyd-steinberg',
                            help='Which image binarization algorithm to use.')
    img_parser.add_argument('--show-preview', action='store_true',
                            help='If set, displays the final image and asks the user for confirmation before printing.')
    img_parser.add_argument('--devicename', type=str, default='GT01',
                            help='Specify the Bluetooth device name to search for. Default value is GT01.')
    img_parser.add_argument('--invert', action='store_true',
                            help='Invert resulting pixels.')
    img_parser.set_defaults(func=img)

    ping_parser = subparsers.add_parser('ping')
    ping_parser.set_defaults(func=ping)

    char_parser = subparsers.add_parser('char')
    char_parser.add_argument('string', type=str)
    char_parser.add_argument(
        '--font', type=Font, default=Font.SIXCAPS_120, choices=Font)
    char_parser.add_argument(
        '--invert', action="store_true")
    char_parser.set_defaults(func=char)

    str_parser = subparsers.add_parser('str')
    str_parser.add_argument(
        '--font', type=Font, default=Font.DIALOG_16, choices=Font)
    str_parser.add_argument(
        '--invert', action="store_true")
    str_parser.add_argument('y', type=int)
    str_parser.add_argument('x', type=int)
    str_parser.add_argument('string', type=str)
    str_parser.set_defaults(func=string)

    sleep_parser = subparsers.add_parser('sleep')
    sleep_parser.add_argument(
        '--wait-ms',
        help="Amount of time in milliseconds for a node to wait before forwarding \
        this message to the next node. Be generous if transport is BLE (~5s). If \
        transport is UART, ~100 ms should suffice.",
        type=int, default=5000)
    sleep_parser.set_defaults(func=sleep)

    return args.parse_args()


def make_logger(log_level):
    logger = logging.getLogger('elink-client')
    logger.setLevel(log_level)
    h = logging.StreamHandler(sys.stdout)
    h.setLevel(log_level)
    logger.addHandler(h)
    return logger


def make_transport(args, logger):
    if args.transport == 'uart':
        return send_serial
    elif args.transport == 'ble':
        async def f(msgs):
            device = await scan("elink", logger, timeout=30)
            return await send_ble(device, msgs, logger)
        return f
        # print(device)
        # return lambda msgs: send_ble('EFC7BBF8-A73D-4C27-AC24-2F10926B1DDB', msgs, logger)
    else:
        raise RuntimeError(f'Invalid transport type: {args.transport}')


def main():
    args = parse_args()

    log_level = getattr(logging, args.log_level.upper())
    logger = make_logger(log_level)

    # args.send_message = asyncio.run(make_transport(args, logger))
    args.send_message = make_transport(args, logger)

    return args.func(args, logger)


if __name__ == '__main__':
    main()
