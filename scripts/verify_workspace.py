"""Check the relationships between reports, copies and decompiler outputs."""
import csv
import json
from pathlib import Path
from inventory import ROOT, PE, sha256


def main():
    inventory = json.loads((ROOT / "analysis/inventory.json").read_text(encoding="utf-8"))
    checks = []
    for binary in inventory["binaries"]:
        local = ROOT / "inputs" / binary["path"]
        if sha256(local) != binary["sha256"]:
            raise ValueError(f"Original input has changed: {local}")
        checks.append(f"Verified input SHA-256: {binary['path']}")
    provenance = json.loads((ROOT / "analysis/runtime-provenance.json").read_text())
    image = ROOT / "inputs/runtime/FarmingSimulator2015Game.runtime.bin"
    if sha256(image) != provenance["analysis_image_sha256"]:
        raise ValueError("Runtime image changed after capture")
    original = (ROOT / "inputs/x64/FarmingSimulator2015Game.exe").read_bytes()
    captured = image.read_bytes()
    pe = PE(original)
    code = next(s for s in pe.sections if s["name"] == ".text")
    start, stop = code["raw_offset"], code["raw_offset"] + provenance["captured_bytes"]
    if len(original) != len(captured) or original[:start] != captured[:start] or original[stop:] != captured[stop:]:
        raise ValueError("Runtime capture changed bytes outside the documented code section")
    if original[start:stop] == captured[start:stop]:
        raise ValueError("Runtime code unexpectedly identical to protected disk code")
    checks.append("Runtime image: only documented .text bytes changed")
    reference = ROOT / "inputs/reference/rle8-reference.bin"
    reference_metadata = json.loads((ROOT / "analysis/rle8-reference.json").read_text())
    if sha256(reference) != reference_metadata["sha256"]:
        raise ValueError("Native RLE reference changed")
    address = int(reference_metadata["entry"], 16)
    offset = pe.offset(address - pe.base)
    if reference.read_bytes() != captured[offset:offset + reference_metadata["length"]]:
        raise ValueError("Native RLE reference differs from the captured game code")
    checks.append("RLE reference matches exactly the original captured native function")
    for out in sorted((ROOT / "recovered/decompiled").iterdir()):
        if not (out / "functions.csv").is_file():
            continue
        target = out.name
        with (out / "functions.csv").open(encoding="utf-8", newline="") as stream:
            functions = {row["entry"] for row in csv.DictReader(stream)}
        with (out / "decompiled-index.csv").open(encoding="utf-8", newline="") as stream:
            results = list(csv.DictReader(stream))
        ok = 0
        for result in results:
            if result["entry"] not in functions:
                raise ValueError(f"Uncatalogued export: {target} {result['entry']}")
            if result["status"] == "ok":
                source = (out / result["file"]).read_text(encoding="utf-8")
                if result["entry"] not in source or "not buildable source" not in source:
                    raise ValueError(f"Missing provenance: {target} {result['file']}")
                ok += 1
        checks.append(f"{target}: {len(functions)} catalogued, {ok} successful pseudocode exports")
    result = dict(status="passed", checks=checks)
    (ROOT / "analysis/verification.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
