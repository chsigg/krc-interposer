import os
from intelhex import IntelHex

# Configuration
HEX_FILE = "pic/firmware.hex" # Path to PIC hex file
OUTPUT_HEADER = "pic/firmware_data.h"
FLASH_START = 0x0000
FLASH_END = 0x0FFF
ROW_SIZE = 32

def generate_header():
    if not os.path.exists(HEX_FILE):
        print(f"Waiting for hex file: {HEX_FILE}")
        return

    ih = IntelHex(HEX_FILE)
    
    # PIC16 words are 14-bit, but stored as 16-bit (2 bytes) in the hex file.
    # IntelHex byte addresses are 2x the PIC word addresses.
    header_lines = [
        "// AUTO-GENERATED FILE - DO NOT EDIT",
        "#pragma once",
        "#include <stdint.h>",
        "",
        "const uint16_t firmware_data[] = {"
    ]

    words = []
    # Read word by word
    for word_addr in range(FLASH_START, FLASH_END + 1):
        byte_addr = word_addr * 2
        # Read little-endian 16-bit word
        low_byte = ih[byte_addr]
        high_byte = ih[byte_addr + 1]
        word = (high_byte << 8) | low_byte
        
        # Unprogrammed flash is 0x3FFF
        if word == 0xFFFF: 
            word = 0x3FFF
            
        words.append(word)

    # Pad array to ensure it's a multiple of ROW_SIZE (32)
    while len(words) % ROW_SIZE != 0:
        words.append(0x3FFF)

    # Format nicely in rows of 8
    for i in range(0, len(words), 8):
        row = words[i:i+8]
        row_str = "    " + ", ".join([f"0x{w:04X}" for w in row]) + ","
        header_lines.append(row_str)

    header_lines.extend([
        "};",
        f"const uint32_t firmware_words = {len(words)};",
        ""
    ])

    with open(OUTPUT_HEADER, "w") as f:
        f.write("\n".join(header_lines))
    print(f"Generated {OUTPUT_HEADER} with {len(words)} words.")

if __name__ == "__main__":
    generate_header()
