BIN_AUDIO_PCM = 0x01
BIN_IMAGE_JPEG = 0x02
BIN_AUDIO_MP3 = 0x03
BIN_FW_CHUNK = 0x04
BIN_SNAPSHOT = 0x05


def frame(opcode, payload):
    return bytes([opcode]) + payload


def split_frame(raw):
    return raw[0], raw[1:]