"""Index the named Lua registrations visible in the recovered registration routine.

The pattern is specific to the current Ghidra export of 0x140011a30. It records
observed name/implementation/wrapper associations, not reconstructed signatures.
"""
import json
import re
from inventory import ROOT


def main():
    path = ROOT / "recovered/decompiled/game-focus/140011a30_FUN_140011a30.c"
    code = path.read_text(encoding="utf-8")
    pattern = re.compile(
        r'local_20 = "([^"\\]+)";\s+local_18 = (FUN_[0-9a-f]+);'
        r'.*?FUN_140031bc0\(DAT_14162bc88,(FUN_[0-9a-f]+),', re.S)
    bindings = [dict(name=m.group(1), implementation="0x" + m.group(2)[4:],
                     wrapper="0x" + m.group(3)[4:], registration_function="0x140011a30")
                for m in pattern.finditer(code)]
    if not bindings or bindings[0]["name"] != "source":
        raise ValueError("Recovered registration format has changed; inspect manually")
    output = ROOT / "analysis/lua-bindings.json"
    output.write_text(json.dumps(dict(scope="Single observed registration routine; not all game APIs",
                                     bindings=bindings), indent=2) + "\n", encoding="utf-8")
    print(f"Indexed {len(bindings)} observed Lua registrations")


if __name__ == "__main__":
    main()
