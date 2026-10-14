"""Read-only inventory of FS15; work only on local copies of the binaries."""
import argparse
from collections import Counter
import csv
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_GAME = Path(os.environ.get("FS15_GAME_DIR", r"C:\Program Files (x86)\Steam\steamapps\common\Farming Simulator 15"))


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def entropy(data):
    if not data:
        return 0.0
    size = len(data)
    return round(-sum((n / size) * math.log2(n / size) for n in Counter(data).values()), 4)


class PE:
    """Small PE reader for provenance, addressing, imports, exports and sections."""
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("Not a DOS/PE image")
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("Missing PE signature")
        self.machine, nsections, self.timestamp = struct.unpack_from("<HHI", data, pe + 4)
        opt_size = struct.unpack_from("<H", data, pe + 20)[0]
        opt = pe + 24
        self.bits = {0x10b: 32, 0x20b: 64}[struct.unpack_from("<H", data, opt)[0]]
        self.entry_rva = struct.unpack_from("<I", data, opt + 16)[0]
        self.base = struct.unpack_from("<Q" if self.bits == 64 else "<I", data,
                                       opt + (24 if self.bits == 64 else 28))[0]
        self.size_headers = struct.unpack_from("<I", data, opt + 60)[0]
        self.subsystem = struct.unpack_from("<H", data, opt + 68)[0]
        directory_start = opt + (112 if self.bits == 64 else 96)
        ndirs = struct.unpack_from("<I", data, directory_start - 4)[0]
        self.directories = [struct.unpack_from("<II", data, directory_start + i * 8)
                            for i in range(min(ndirs, 16))]
        self.sections = []
        for i in range(nsections):
            off = opt + opt_size + i * 40
            name = data[off:off + 8].split(b"\0")[0].decode("ascii", errors="replace")
            vsize, rva, size, raw = struct.unpack_from("<IIII", data, off + 8)
            flags = struct.unpack_from("<I", data, off + 36)[0]
            self.sections.append(dict(name=name, rva=rva, virtual_size=vsize,
                                      raw_offset=raw, raw_size=size,
                                      flags=f"0x{flags:08x}",
                                      entropy=entropy(data[raw:raw + size])))

    def offset(self, rva):
        if 0 <= rva < self.size_headers:
            return rva
        for section in self.sections:
            delta = rva - section["rva"]
            if 0 <= delta < section["raw_size"]:
                return section["raw_offset"] + delta
        raise ValueError(f"RVA without file backing: {rva:x}")

    def address(self, offset):
        for section in self.sections:
            if section["raw_offset"] <= offset < section["raw_offset"] + section["raw_size"]:
                rva = section["rva"] + offset - section["raw_offset"]
                return rva, self.base + rva, section["name"]
        return None, None, ""

    def cstr(self, offset):
        end = self.data.find(b"\0", offset)
        if end < 0:
            raise ValueError("Unterminated string")
        return self.data[offset:end].decode("ascii", errors="replace")

    def imports(self):
        if len(self.directories) < 2 or not self.directories[1][0]:
            return []
        rows = []
        descriptor = self.offset(self.directories[1][0])
        step = self.bits // 8
        while True:
            original, timestamp, chain, name_rva, thunk = struct.unpack_from("<IIIII", self.data, descriptor)
            if not any((original, timestamp, chain, name_rva, thunk)):
                break
            dll = self.cstr(self.offset(name_rva))
            table = self.offset(original or thunk)
            functions = []
            index = 0
            while True:
                value = struct.unpack_from("<Q" if self.bits == 64 else "<I", self.data, table + index * step)[0]
                if value == 0:
                    break
                ordinal = bool(value & (1 << (self.bits - 1)))
                name = f"ordinal:{value & 0xffff}" if ordinal else self.cstr(self.offset(value) + 2)
                functions.append(dict(name=name, iat_va=f"0x{self.base + thunk + index * step:x}"))
                index += 1
            rows.append(dict(dll=dll, functions=functions))
            descriptor += 20
        return rows

    def exports(self):
        if not self.directories or not self.directories[0][0]:
            return []
        start = self.offset(self.directories[0][0])
        base, nfuncs, nnames, funcs, names, ords = struct.unpack_from("<IIIIII", self.data, start + 16)
        output = []
        for i in range(nnames):
            name_rva = struct.unpack_from("<I", self.data, self.offset(names) + i * 4)[0]
            ordinal = struct.unpack_from("<H", self.data, self.offset(ords) + i * 2)[0]
            rva = struct.unpack_from("<I", self.data, self.offset(funcs) + ordinal * 4)[0]
            output.append(dict(name=self.cstr(self.offset(name_rva)), ordinal=base + ordinal,
                               va=f"0x{self.base + rva:x}"))
        return output

    def report(self):
        exceptions = self.directories[3] if len(self.directories) > 3 else (0, 0)
        return dict(bits=self.bits, machine=f"0x{self.machine:04x}",
                    image_base=f"0x{self.base:x}", entry_rva=f"0x{self.entry_rva:x}",
                    entry_va=f"0x{self.base + self.entry_rva:x}",
                    coff_timestamp=self.timestamp, subsystem=self.subsystem,
                    sections=self.sections, imports=self.imports(), exports=self.exports(),
                    x64_runtime_function_records=exceptions[1] // 12 if self.bits == 64 else None)


def strings(data, pe):
    for pattern, encoding in [(rb"[\x20-\x7e]{6,}", "ascii"),
                              (rb"(?:[\x20-\x7e]\x00){6,}", "utf-16-le")]:
        for match in re.finditer(pattern, data):
            rva, va, section = pe.address(match.start())
            yield dict(offset=f"0x{match.start():x}", rva=f"0x{rva:x}" if rva is not None else "",
                       va=f"0x{va:x}" if va is not None else "", section=section,
                       encoding=encoding, text=match.group().decode(encoding))


def write_json(path, obj):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--game", type=Path, default=DEFAULT_GAME)
    args = parser.parse_args()
    game = args.game.resolve(strict=True)
    if game == ROOT or ROOT.is_relative_to(game):
        raise ValueError("Analysis workspace must be separate from the installed game")
    output = ROOT / "analysis"
    output.mkdir(exist_ok=True)
    inputs = ROOT / "inputs"
    inputs.mkdir(exist_ok=True)
    shutil.copy2(game / "FarmingSimulator2015.xml", inputs / "FarmingSimulator2015.xml")
    files = []
    for path in sorted(game.rglob("*")):
        if path.is_file():
            files.append(dict(path=path.relative_to(game).as_posix(), size=path.stat().st_size,
                              extension=path.suffix.lower()))
    write_json(output / "files.json", files)
    targets = ["x64/FarmingSimulator2015Game.exe", "x86/FarmingSimulator2015Game.exe",
               "FarmingSimulator2015.exe", "dedicatedServer.exe"]
    binaries = []
    for relative in targets:
        source = game / relative
        local = inputs / relative
        local.parent.mkdir(parents=True, exist_ok=True)
        source_hash = sha256(source)
        if not local.exists() or sha256(local) != source_hash:
            shutil.copy2(source, local)
        if sha256(local) != source_hash:
            raise ValueError(f"Copy verification failed: {relative}")
        data = local.read_bytes()
        pe = PE(data)
        report = pe.report()
        report.update(path=relative, size=len(data), sha256=source_hash)
        label = relative.replace("/", "_").removesuffix(".exe")
        write_json(output / "pe" / f"{label}.json", report)
        rows = list(strings(data, pe))
        string_path = output / "strings" / f"{label}.csv"
        string_path.parent.mkdir(exist_ok=True)
        with string_path.open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.DictWriter(stream, fieldnames=["offset", "rva", "va", "section", "encoding", "text"])
            writer.writeheader()
            writer.writerows(rows)
        candidates = [row for row in rows if re.search(
            r"\.lua\b|\.pdb\b|\.cpp\b|\.gar\b|lua[A-Z_]|GIANTS Engine|^Lua|^LUA|^\.?AV", row["text"])]
        write_json(output / "strings" / f"{label}_anchors.json", candidates)
        report["strings_count"] = len(rows)
        report["anchors_count"] = len(candidates)
        binaries.append(report)
        print(f"{relative}: {pe.bits} bit, {len(rows)} strings, {len(candidates)} anchors", flush=True)
    archives = []
    for name in ("dataS.gar", "dataS2.gar"):
        source = game / name
        with source.open("rb") as stream:
            header = stream.read(256)
        archives.append(dict(path=name, size=source.stat().st_size, sha256=sha256(source),
                             header_hex=header.hex(), first_256_entropy=entropy(header),
                             recognized_zip=zipfile.is_zipfile(source)))
    sdk_manifest = []
    for name in ("sampleMod.zip", "modMapSDK.zip"):
        source = game / "sdk" / name
        local = inputs / "sdk" / name
        local.parent.mkdir(exist_ok=True)
        shutil.copy2(source, local)
        destination = (ROOT / "recovered" / "sdk" / source.stem).resolve()
        destination.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(local) as archive:
            for entry in archive.infolist():
                target = (destination / entry.filename).resolve()
                if not target.is_relative_to(destination):
                    raise ValueError(f"Unsafe SDK ZIP path: {entry.filename}")
                archive.extract(entry, destination)
                sdk_manifest.append(dict(archive=name, path=entry.filename, size=entry.file_size,
                                         crc32=f"{entry.CRC:08x}"))
    write_json(output / "sdk-manifest.json", sdk_manifest)
    state = dict(created_utc=datetime.now(timezone.utc).isoformat(), game_root=str(game),
                 version=(game / "VERSION").read_text().strip(), file_count=len(files),
                 total_bytes=sum(row["size"] for row in files),
                 extensions=dict(Counter(row["extension"] for row in files)),
                 binaries=binaries, archives=archives,
                 sdk_entries=len(sdk_manifest),
                 sdk_lua=[row["path"] for row in sdk_manifest if row["path"].endswith(".lua")])
    write_json(output / "inventory.json", state)
    print(json.dumps({k: state[k] for k in ["version", "file_count", "total_bytes", "extensions", "sdk_lua"]}, indent=2))


if __name__ == "__main__":
    main()
