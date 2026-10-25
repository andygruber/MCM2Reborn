"""release_inputs.py: what a release build needs beside the repository (.github/workflows/release-packages.yml).

  retail <dir>    Mcm2.exe and the game's UI folder (its profiles left out), from the base game the
                  original project hosts: MCM2-base.zip on xshxdex98/MCM2Reborn's release mcm2-base,
                  the same download the launcher offers. The exe is checked against docs/binary.md's
                  sha256, the one every address in this project is for.
  baseline <dir>  the original project's newest release's package, as package.zip, with its tag and
                  repository in tag.txt and repo.txt. The release is that package with this build's
                  files over it (tools/release.py --over); everything else - eos.ini, the EOS helper
                  and its DLL, the menus' photos and fonts, the shipped content - is as it has it.

GITHUB_TOKEN, when set, is sent to GitHub's API.
"""
import hashlib
import json
import os
import re
import shutil
import sys
import tempfile
import urllib.request
import zipfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ORIGINAL = "xshxdex98/MCM2Reborn"
BASE_GAME = "https://github.com/%s/releases/download/mcm2-base/MCM2-base.zip" % ORIGINAL
PACKAGE = re.compile(r"^MCM2Reborn-.*\.zip$", re.I)


def fetch(url, to=None, api=False):
    headers = {"User-Agent": "MCM2Reborn-release-build"}
    if api:
        headers["Accept"] = "application/vnd.github+json"
        if os.environ.get("GITHUB_TOKEN"):
            headers["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers)) as answer:
        if to is None:
            return answer.read()
        with open(to, "wb") as f:
            shutil.copyfileobj(answer, f, 1 << 20)


def retail(out):
    text = (REPO / "docs" / "binary.md").read_text(encoding="utf-8")
    expected = re.search(r"sha256\s+([0-9a-f]{64})", text).group(1)
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temp:
        base = Path(temp) / "MCM2-base.zip"
        print("downloading", BASE_GAME, flush=True)
        fetch(BASE_GAME, base)
        with zipfile.ZipFile(base) as z:
            exe = [i for i in z.infolist() if i.filename.lower() == "mcm2.exe"]
            if not exe:
                sys.exit("no Mcm2.exe in MCM2-base.zip")
            data = z.read(exe[0])
            if hashlib.sha256(data).hexdigest() != expected:
                sys.exit("MCM2-base.zip's Mcm2.exe is not the one docs/binary.md names (sha256 %s)" % expected)
            (out / "Mcm2.exe").write_bytes(data)
            ui = [i for i in z.infolist() if not i.is_dir() and i.filename.lower().startswith("ui/")
                  and not i.filename.lower().startswith("ui/profile/")]
            for info in ui:
                info.filename = "UI/" + info.filename[3:]          # one spelling, whatever the zip's
                z.extract(info, out)
    print("Mcm2.exe (sha256 %s) and %d files of UI in %s" % (expected, len(ui), out))


def releases(repo):
    found = json.loads(fetch("https://api.github.com/repos/%s/releases?per_page=30" % repo, api=True))
    return found if isinstance(found, list) else []


def baseline(out):
    for release in releases(ORIGINAL):                              # the newest first
        if release["draft"] or release["prerelease"]:
            continue
        url = next((a["browser_download_url"] for a in release["assets"] if PACKAGE.match(a["name"])), None)
        if url:
            out.mkdir(parents=True, exist_ok=True)
            print("downloading", url, flush=True)
            fetch(url, out / "package.zip")
            zipfile.ZipFile(out / "package.zip").testzip()
            (out / "tag.txt").write_text(release["tag_name"])
            (out / "repo.txt").write_text(ORIGINAL)
            print("the baseline is %s %s" % (ORIGINAL, release["tag_name"]))
            return
    sys.exit("no release with a package on " + ORIGINAL)


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in ("retail", "baseline"):
        sys.exit(__doc__)
    {"retail": retail, "baseline": baseline}[sys.argv[1]](Path(sys.argv[2]))


if __name__ == "__main__":
    main()
