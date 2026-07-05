"""
osoLogic — osoST Python Compiler
ostc/hex_writer.py — Intel HEX emitter compatible with STLite pcodevm

Copyright (C) 2026 Angel Miguel Zúñiga Schmemund <miguel@ibercomp.com>
              Ibercomp SL, Roig Borrell SL

Part of the osoLogic open-source PLC project — osologic.org
SPDX-License-Identifier: AGPL-3.0-or-later

Produces Intel HEX records with the STLite executable header prepended.

Header layout (24 bytes, little-endian) / Cabecera (24 bytes, little-endian):
  offset  0: uint16  magic          = 0xA55A
  offset  2: uint32  app_id         = 0x00000001
  offset  6: uint16  version        = 0x0100
  offset  8: uint32  code_start     = 24 (= HEADER_SIZE)
  offset 12: uint32  stack_needed   = minimum stack bytes
  offset 16: uint32  global_bytes   = bytes for global variables
  offset 20: uint32  minimum_ram_size

Usage / Uso:
  from ostc.hex_writer import HexWriter

  w = HexWriter(global_bytes=8, stack_needed=256)
  w.emit(PUSH_I)
  w.emit(VT_I32)
  w.emit_i32(42)
  w.emit(HALT)
  print(w.to_intel_hex())
"""

from __future__ import annotations
import struct
from io import BytesIO

MAGIC         = 0xA55A
APP_ID        = 0x00000001
VERSION       = 0x0100
HEADER_SIZE   = 24
RECORDS_PER_LINE = 16  # bytes per HEX data record


def _checksum(data: bytes) -> int:
    """Intel HEX record checksum. / Checksum de registro Intel HEX."""
    return ((~sum(data) + 1) & 0xFF)


def _hex_record(record_type: int, address: int, data: bytes) -> str:
    """
    Build one Intel HEX record line.
    Construye una línea de registro Intel HEX.
    """
    count = len(data)
    body  = bytes([count, (address >> 8) & 0xFF, address & 0xFF, record_type]) + data
    cks   = _checksum(body)
    return ":" + body.hex().upper() + f"{cks:02X}"


class HexWriter:
    """
    Accumulates P-code bytes and emits Intel HEX with STLite header.
    Acumula bytes P-code y emite Intel HEX con cabecera STLite.
    """

    def __init__(self, global_bytes: int = 0, stack_needed: int = 512,
                 minimum_ram_size: int = 2048):
        self._buf           = BytesIO()
        self._global_bytes  = global_bytes
        self._stack_needed  = stack_needed
        self._min_ram       = max(minimum_ram_size, global_bytes + stack_needed + 256)

    # ── Emission primitives / Primitivas de emisión ───────────────

    def emit(self, byte: int) -> "HexWriter":
        """Emit one byte. / Emitir un byte."""
        self._buf.write(bytes([byte & 0xFF]))
        return self

    def emit_u8(self, v: int) -> "HexWriter":
        return self.emit(v & 0xFF)

    def emit_i8(self, v: int) -> "HexWriter":
        return self.emit(struct.pack("b", v)[0])

    def emit_u16(self, v: int) -> "HexWriter":
        self._buf.write(struct.pack("<H", v & 0xFFFF))
        return self

    def emit_i16(self, v: int) -> "HexWriter":
        self._buf.write(struct.pack("<h", v))
        return self

    def emit_i32(self, v: int) -> "HexWriter":
        self._buf.write(struct.pack("<i", v))
        return self

    def emit_f32(self, v: float) -> "HexWriter":
        self._buf.write(struct.pack("<f", v))
        return self

    def emit_string(self, s: str, max_len: int = 80) -> "HexWriter":
        """
        Emit a length-prefixed string (STLite format: 1 byte max_len, then chars).
        Emitir cadena con prefijo de longitud (formato STLite: 1 byte max, luego chars).
        """
        encoded = s.encode("utf-8", errors="replace")[:max_len]
        self._buf.write(bytes([max_len]))
        self._buf.write(encoded)
        self._buf.write(b"\x00" * (max_len - len(encoded)))
        return self

    def patch_i32(self, offset: int, value: int) -> None:
        """
        Patch a 32-bit value at a previously emitted offset (for back-patching jumps).
        Parchear un valor de 32 bits en un offset ya emitido (para back-patching de saltos).
        """
        pos = self._buf.tell()
        self._buf.seek(offset)
        self._buf.write(struct.pack("<i", value))
        self._buf.seek(pos)

    def patch_i16(self, offset: int, value: int) -> None:
        """Patch a signed 16-bit value (relative jump offset) at a previous offset."""
        pos = self._buf.tell()
        self._buf.seek(offset)
        self._buf.write(struct.pack("<h", value))
        self._buf.seek(pos)

    def patch_u16(self, offset: int, value: int) -> None:
        """Patch an unsigned 16-bit value (absolute address) at a previous offset."""
        pos = self._buf.tell()
        self._buf.seek(offset)
        self._buf.write(struct.pack("<H", value & 0xFFFF))
        self._buf.seek(pos)

    def position(self) -> int:
        """Current code offset (relative to code_start). / Offset de código actual."""
        return self._buf.tell()

    # ── Header / Cabecera ─────────────────────────────────────────

    def _build_header(self, code_size: int) -> bytes:
        """
        Build the 24-byte STLite executable header.
        Construir la cabecera ejecutable STLite de 24 bytes.
        """
        min_ram = max(self._min_ram,
                      self._global_bytes + self._stack_needed + code_size // 2 + 256)
        return struct.pack("<HIHI III",
            MAGIC,
            APP_ID,
            VERSION,
            0,            # padding to align code_start to offset 8
            HEADER_SIZE,  # code_start
            self._stack_needed,
            self._global_bytes,
            min_ram,
        )

    # Wait — let me build it field by field to match the exact layout
    def _build_header_v2(self) -> bytes:
        return struct.pack("<H I H I I I I",
            MAGIC,                  # 0: uint16  magic
            APP_ID,                 # 2: uint32  app_id
            VERSION,                # 6: uint16  version
            HEADER_SIZE,            # 8: uint32  code_start
            self._stack_needed,     # 12: uint32 stack_needed
            self._global_bytes,     # 16: uint32 global_bytes
            self._min_ram,          # 20: uint32 minimum_ram_size
        )  # total = 2+4+2+4+4+4+4 = 24 bytes ✓

    # ── Intel HEX output / Salida Intel HEX ──────────────────────

    def to_intel_hex(self) -> str:
        """
        Serialize to Intel HEX string (includes STLite header + P-code).
        Serializar a string Intel HEX (incluye cabecera STLite + P-code).
        """
        header  = self._build_header_v2()
        payload = self._buf.getvalue()
        image   = header + payload

        lines: list[str] = []
        offset = 0
        while offset < len(image):
            chunk = image[offset:offset + RECORDS_PER_LINE]
            lines.append(_hex_record(0x00, offset, chunk))
            offset += len(chunk)

        lines.append(_hex_record(0x01, 0, b""))  # EOF record
        return "\n".join(lines) + "\n"

    def to_bytes(self) -> bytes:
        """Return raw image bytes (header + P-code). / Devolver bytes imagen."""
        return self._build_header_v2() + self._buf.getvalue()
