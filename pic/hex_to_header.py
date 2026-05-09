import os
from intelhex import IntelHex

# Configuration
HEX_FILE = "pic/firmware.hex" # Path to PIC hex file
OUTPUT_HEADER = "pic/firmware_data.h"
FLASH_START = 0x0000
FLASH_END = 0x0FFF
ROW_SIZE = 32

CONFIG_START = 0x8007
CONFIG_END = 0x800B # Configuration Space Only

def extract_words(ih, start, end, pad_to_multiple=1):
    words = []
    for word_addr in range(start, end + 1):
        byte_addr = word_addr * 2
        try:
            low_byte = ih[byte_addr]
            high_byte = ih[byte_addr + 1]
            word = (high_byte << 8) | low_byte
        except IndexError:
             word = 0x3FFF # Unprogrammed word

        if word == 0xFFFF:
            word = 0x3FFF
        words.append(word)

    while len(words) % pad_to_multiple != 0:
        words.append(0x3FFF)
    return words

def format_array(name, words):
    lines = [f"const uint16_t {name}[] = {{"]
    for i in range(0, len(words), 8):
        row = words[i:i+8]
        row_str = "    " + ", ".join([f"0x{w:04X}" for w in row]) + ","
        lines.append(row_str)
    lines.append("};")
    lines.append("")
    return lines

def generate_header():
    if not os.path.exists(HEX_FILE):
        print(f"Waiting for hex file: {HEX_FILE}")
        return

    ih = IntelHex(HEX_FILE)

    flash_words = extract_words(ih, FLASH_START, FLASH_END, ROW_SIZE)
    config_words = extract_words(ih, CONFIG_START, CONFIG_END, 1)

    header_lines = [
        "// AUTO-GENERATED FILE - DO NOT EDIT",
        "#pragma once",
        "#include <stdint.h>",
        ""
    ]
    header_lines.extend(format_array("kProgramData", flash_words))
    header_lines.extend(format_array("kConfigData", config_words))

    with open(OUTPUT_HEADER, "w") as f:
        f.write("\n".join(header_lines))
    print(f"Generated {OUTPUT_HEADER} with {len(flash_words)} flash words and {len(config_words)} config words.")

if __name__ == "__main__":
    generate_header()
