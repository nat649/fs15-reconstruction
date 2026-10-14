"""Download a pinned official Ghidra release into this workspace only."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TAG = "Ghidra_11.4.2_build"
SHA256 = "795a02076af16257bd6f3f4736c4fc152ce9ff1f95df35cd47e2adc086e037a6"


def main():
    ready = list((ROOT / "tools").glob("ghidra_11.4.2_PUBLIC/support/analyzeHeadless.bat"))
    if ready:
        print(f"Already available: {ready[0]}", flush=True)
        return
    api = f"https://api.github.com/repos/NationalSecurityAgency/ghidra/releases/tags/{TAG}"
    req = urllib.request.Request(api, headers={"User-Agent": "FS15-local-research"})
    with urllib.request.urlopen(req, timeout=45) as response:
        release = json.load(response)
    asset = next(a for a in release["assets"] if a["name"].endswith(".zip"))
    destination = ROOT / "tools" / "downloads" / asset["name"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        url = asset["browser_download_url"]
        if not url.startswith(f"https://github.com/NationalSecurityAgency/ghidra/releases/download/{TAG}/"):
            raise ValueError("Unexpected asset origin")
        temporary = destination.with_suffix(".partial")
        print(f"Downloading official release: {asset['name']} ({asset['size']} bytes)", flush=True)
        with urllib.request.urlopen(url, timeout=60) as response, temporary.open("wb") as out:
            size = 0
            last_bucket = -1
            while block := response.read(1024 * 1024):
                out.write(block)
                size += len(block)
                bucket = size // (64 * 1024 * 1024)
                if bucket != last_bucket:
                    print(f"Downloaded {size // (1024 * 1024)} MiB", flush=True)
                    last_bucket = bucket
        if size != asset["size"]:
            raise ValueError(f"Truncated download: {size} != {asset['size']}")
        temporary.replace(destination)
    with destination.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != SHA256:
        raise ValueError(f"SHA-256 mismatch: {digest}")
    print(f"SHA-256 verified: {digest}", flush=True)
    tools_root = (ROOT / "tools").resolve()
    with zipfile.ZipFile(destination) as archive:
        for entry in archive.infolist():
            target = (tools_root / entry.filename).resolve()
            if not target.is_relative_to(tools_root):
                raise ValueError(f"Unsafe ZIP entry: {entry.filename}")
        archive.extractall(tools_root)
    (ROOT / "analysis").mkdir(exist_ok=True)
    (ROOT / "analysis" / "ghidra-tool.json").write_text(json.dumps({
        "tag": TAG, "asset": asset["name"], "url": asset["browser_download_url"],
        "sha256": digest, "release_url": release["html_url"],
    }, indent=2) + "\n", encoding="utf-8")
    print("Ghidra ready.", flush=True)


if __name__ == "__main__":
    main()
