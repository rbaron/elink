import struct

from txt import Font

MAGIC_BYTE = b'\x42'


_FONTS = {
    Font.SIXCAPS_120: 0x00,
    Font.ROBOTO_MONO_100: 0x01,
    Font.DIALOG_16: 0x02,
    Font.SPECIAL_ELITE_30: 0x03,
    Font.OPEN_SANS_8: 0x04,
    Font.OPEN_SANS_BOLD_8: 0x05,
    Font.OPEN_SANS_14: 0x06,
    Font.OPEN_SANS_BOLD_14: 0x07,
    Font.DSEG14_90: 0x08,
    Font.PRESS_START_50: 0x09
}


def chunkify(data, chunk_size):
    return (
        data[i: i + chunk_size] for i in range(0, len(data), chunk_size)
    )


def cmd_img(to: int, img: bytes) -> bytes:
    size = len(img)
    print(f'Image has {size} bytes')
    yield bytes([to]) + b'\x01' + struct.pack('>H', size) + img


def cmd_ping(to: int):
    yield bytes([to]) + b'\x03\x00\x00'


def cmd_invert_buffer(to: int):
    yield bytes([to]) + b'\x07\x00\x00'


def cmd_clear_buffer(to: int):
    yield bytes([to]) + b'\x00\x00\x00'


def cmd_push_buffer(to: int):
    yield bytes([to]) + b'\x06\x00\x00'


def get_font_byte(font: Font):
    return _FONTS[font]


def cmd_char(font: Font, string: str, invert: bool):
    for i, c in reversed(list(enumerate(string))):
        yield bytes([i]) + b'\x04\x00\x03' + \
            bytes([get_font_byte(font)]) + c.encode('utf-8') + bytes([invert])


def cmd_str(to: int, font: Font, base_y: int, base_x: int, string: str, invert: bool):
    yield from cmd_clear_buffer(to)
    # 1     font
    # 2 - 3 base_y
    # 4 - 5 base_x
    # 6 - x string
    # x+1 \0
    size = 1 + 2 + 2 + len(string) + 1
    yield bytes([to]) + b'\x05' + struct.pack('>H', size) + \
        bytes([get_font_byte(font)]) + \
        struct.pack('>h', base_y) + \
        struct.pack('>h', base_x) + \
        string.encode("utf-8") + b'\x00'

    if invert:
        yield from cmd_invert_buffer()

    yield from cmd_push_buffer(to)


def cmd_sleep(to: int, wait_ms: int):
    yield bytes([to]) + b'\x08\x00\x02' + struct.pack('>H', wait_ms)
