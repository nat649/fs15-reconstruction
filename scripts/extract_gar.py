"""Extract a local FS15 1.4.2.0 GAR using its verified native cipher reference."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from gar import GarArchive, safe_relative_name, normalize_luajit
from inventory import ROOT, sha256, write_json
from native_gar import NativeGarCipher, DEFAULT_IMAGE, EXPECTED_IMAGE


def output_path(root, name):
    relative = safe_relative_name(name)
    target = root.joinpath(*relative.parts).resolve()
    if not target.is_relative_to(root):
        raise ValueError(f"Output escapes extraction root: {name}")
    target.parent.mkdir(parents=True, exist_ok=True)
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--image", type=Path, default=DEFAULT_IMAGE)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--decode-luajit", action="store_true")
    parser.add_argument("--validate-xml", action="store_true")
    args = parser.parse_args()
    archive_path = args.archive.resolve(strict=True)
    destination = (args.output or ROOT / "recovered/gar" / archive_path.stem).resolve()
    destination.mkdir(parents=True, exist_ok=True)
    lua_destination = (ROOT / "recovered/luajit" / archive_path.stem).resolve()
    lua_destination.mkdir(parents=True, exist_ok=True)
    results = []
    counts = Counter()
    with NativeGarCipher(args.image) as cipher, GarArchive(archive_path, cipher.decrypt) as archive:
        for entry in archive.entries:
            data = archive.read(entry)
            output_path(destination, entry.name).write_bytes(data)
            suffix = Path(entry.name).suffix.lower()
            counts[suffix] += 1
            row = dict(name=entry.name, size=len(data), sha256=hashlib.sha256(data).hexdigest())
            if args.validate_xml and suffix in (".xml", ".i3d"):
                try:
                    root = ET.fromstring(data)
                    row["xml_root"] = root.tag
                    counts["validated_xml"] += 1
                except ET.ParseError as error:
                    row["xml_error"] = str(error)
                    counts["xml_parse_errors"] += 1
                    print(f"Strict XML warning: {entry.name}: {error}", flush=True)
            if suffix in (".luc", ".lua") and args.decode_luajit:
                normalized, transformed = normalize_luajit(data, cipher.lua_key)
                output_path(lua_destination, entry.name).write_bytes(normalized)
                row["luajit_transformed"] = transformed
                row["normalized_sha256"] = hashlib.sha256(normalized).hexdigest()
                counts["luajit_transformed" if transformed else "scripts_passthrough"] += 1
            results.append(row)
            if len(results) % 100 == 0:
                print(f"Extracted {len(results)}/{len(archive.entries)} records", flush=True)
        manifest = dict(archive=archive_path.name, archive_sha256=sha256(archive_path),
                        archive_version=f"0x{archive.version:08x}", header_counts=archive.header_counts,
                        native_image_sha256=EXPECTED_IMAGE,
                        cipher="original native reference; not reconstructed", counts=dict(counts), files=results)
    write_json(ROOT / "analysis" / f"{archive_path.stem}-extraction.json", manifest)
    summary = {k: manifest[k] for k in ("archive", "archive_sha256", "archive_version", "header_counts", "cipher", "counts")}
    summary["file_count"] = len(results)
    write_json(ROOT / "analysis" / f"{archive_path.stem}-extraction-summary.json", summary)
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    main()
