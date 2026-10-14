"""Reconstructed GAR 5.3 header/index parser; the buffer cipher is supplied separately."""
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
import struct

RECORD_SIZE = 512
HEADER_SIZE = 512
VERSION = 0x00050003


@dataclass(frozen=True)
class Entry:
    name: str
    encrypted_size: int
    size: int
    offset: int
    flags: int
    record: int


def safe_relative_name(name):
    normalized = name.replace("\\", "/")
    if not normalized or normalized.startswith("/") or ":" in normalized or "\0" in normalized:
        raise ValueError(f"Unsafe archive path: {name!r}")
    pieces = normalized.split("/")
    if any(part in ("", ".", "..") for part in pieces):
        raise ValueError(f"Unsafe archive path: {name!r}")
    reserved = {"CON", "PRN", "AUX", "NUL", "CLOCK$"} | {f"{prefix}{i}" for prefix in ("COM", "LPT") for i in range(1, 10)}
    for part in pieces:
        if part.endswith((".", " ")) or part.split(".", 1)[0].upper() in reserved or any(ord(ch) < 32 or ch in '<>"|?*' for ch in part):
            raise ValueError(f"Unsafe Windows archive path: {name!r}")
    return PurePosixPath(*pieces)


class GarArchive:
    def __init__(self, path, decrypt):
        self.path = Path(path)
        self.stream = self.path.open("rb")
        self.decrypt = decrypt
        try:
            size = self.path.stat().st_size
            header = self.stream.read(HEADER_SIZE)
            if len(header) != HEADER_SIZE or header[0x60:0x64] != b"GAR ":
                raise ValueError("Not a supported GAR archive")
            version, count_base, count_extension = struct.unpack_from("<III", header, 0x64)
            if version != VERSION:
                raise ValueError(f"Unsupported GAR version: 0x{version:08x}")
            # 0x1402a6d70 adds the two counts for versions newer than 0x50002.
            count = count_base + count_extension
            index_end = HEADER_SIZE + count * RECORD_SIZE
            if not count or index_end > size:
                raise ValueError("GAR index extends beyond the archive")
            self.version = version
            self.header_counts = (count_base, count_extension)
            encrypted_index = self.stream.read(count * RECORD_SIZE)
            decoded = decrypt(encrypted_index)
            if len(decoded) != len(encrypted_index):
                raise ValueError("Unexpected decoded index length")
            self.entries = []
            names = set()
            spans = []
            for i in range(count):
                record = decoded[i * RECORD_SIZE:(i + 1) * RECORD_SIZE]
                kind, flags, encrypted_size, plain_size, offset, reserved = struct.unpack_from("<IIQQQQ", record)
                if kind != 1 or flags or reserved:
                    raise ValueError(f"Unsupported GAR record {i}: kind={kind}, flags={flags}, reserved={reserved}")
                raw_name, terminator, _ = record[40:].partition(b"\0")
                if not terminator:
                    raise ValueError(f"Unterminated name in GAR record {i}")
                name = safe_relative_name(raw_name.decode("utf-8")).as_posix()
                if name.casefold() in names:
                    raise ValueError(f"Colliding archive path: {name}")
                names.add(name.casefold())
                if encrypted_size < 8 or encrypted_size % 8 or encrypted_size > 0xffffffff:
                    raise ValueError(f"Invalid encrypted length for {name}")
                if not 1 <= encrypted_size - plain_size <= 8:
                    raise ValueError(f"Unexpected padding length for {name}")
                if offset < index_end or offset + encrypted_size > size:
                    raise ValueError(f"Payload outside the archive: {name}")
                spans.append((offset, offset + encrypted_size, name))
                self.entries.append(Entry(name, encrypted_size, plain_size, offset, flags, i))
            previous_end = index_end
            for start, end, name in sorted(spans):
                if start < previous_end:
                    raise ValueError(f"Overlapping payload: {name}")
                previous_end = end
        except BaseException:
            self.stream.close()
            raise

    def read(self, entry):
        self.stream.seek(entry.offset)
        encrypted = self.stream.read(entry.encrypted_size)
        if len(encrypted) != entry.encrypted_size:
            raise ValueError(f"Truncated payload: {entry.name}")
        return self.decrypt(encrypted)[:entry.size]

    def close(self):
        self.stream.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


def normalize_luajit(data, key):
    """Observed 0x140280060 transform: custom version 3 -> standard version 2."""
    if len(key) != 8:
        raise ValueError("Expected the observed 8-byte script transform key")
    if data[:4] != b"\x1bLJ\x03":
        return data, False
    output = bytearray(data)
    output[3] = 2
    for pos in range(4, len(output)):
        output[pos] = (output[pos] + key[pos & 7] + pos) & 0xff
    return bytes(output), True
