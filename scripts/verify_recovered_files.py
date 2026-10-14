"""Recheck every extracted file hash and independent binary format signatures."""
from collections import Counter
import json
from pathlib import Path
import struct
import zlib
from gar import safe_relative_name
from inventory import ROOT, sha256, write_json


def validate_png(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Invalid PNG signature")
    position = 8
    ended = False
    while position + 12 <= len(data):
        length = struct.unpack_from(">I", data, position)[0]
        end = position + 12 + length
        if end > len(data):
            raise ValueError("PNG chunk exceeds file")
        block = data[position + 4:position + 8 + length]
        crc = struct.unpack_from(">I", data, position + 8 + length)[0]
        if zlib.crc32(block) & 0xffffffff != crc:
            raise ValueError("PNG chunk CRC mismatch")
        position = end
        if block[:4] == b"IEND":
            ended = True
            break
    if not ended:
        raise ValueError("Missing PNG end chunk")


def main():
    counts = Counter()
    summaries = []
    for name in ("dataS", "dataS2"):
        manifest = json.loads((ROOT / "analysis" / f"{name}-extraction.json").read_text(encoding="utf-8"))
        if sha256(ROOT / "inputs/archives" / manifest["archive"]) != manifest["archive_sha256"]:
            raise ValueError(f"Archive copy changed: {name}")
        for row in manifest["files"]:
            relative = safe_relative_name(row["name"])
            path = ROOT / "recovered/gar" / name / relative.as_posix()
            if path.stat().st_size != row["size"] or sha256(path) != row["sha256"]:
                raise ValueError(f"Recovered file changed: {name}/{relative}")
            data = path.read_bytes()
            suffix = path.suffix.lower()
            if suffix == ".png":
                validate_png(data)
                counts["png_crc_valid"] += 1
            elif suffix == ".dds":
                if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
                    raise ValueError(f"Invalid DDS header: {relative}")
                counts["dds_headers_valid"] += 1
            elif suffix == ".wav":
                if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
                    raise ValueError(f"Invalid WAV header: {relative}")
                counts["wav_headers_valid"] += 1
            elif suffix == ".luc":
                if data[:4] != b"\x1bLJ\x02":
                    raise ValueError(f"Unexpected script header: {relative}")
                counts["luajit_v2_headers_valid"] += 1
            counts["files_hash_verified"] += 1
        summaries.append(dict(archive=manifest["archive"], file_count=len(manifest["files"])))
    result = dict(status="passed", counts=dict(counts), archives=summaries,
                  note="File hashes match extraction manifests; PNG CRCs and selected format headers checked independently")
    write_json(ROOT / "analysis/recovered-files-verification.json", result)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
