#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Plan Phase 3 verification: renders every blocks-mode fixture through the
# WASM artifact in a headless browser and compares dimensions and pixel
# checksums against the committed native goldens. Any mismatch fails.
#
# Requires: a built WASM artifact (scripts/build-wasm.sh), the native
# goldens (scripts/capture-goldens.sh), google-chrome, python3, deno.
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
artifact_dir="${renderer_dir}/build/wasm/ttr-renderer"
goldens="${renderer_dir}/tests/native-goldens"
work="$(mktemp -d)"
port="${TTR_SMOKE_PORT:-8917}"
widths=(320 480 720)

for required in "${artifact_dir}/ttr-renderer.js" "${artifact_dir}/ttr-renderer.wasm"; do
    [[ -f "${required}" ]] || {
        echo "missing $(basename "${required}"); run scripts/build-wasm.sh" >&2
        exit 1
    }
done
command -v google-chrome >/dev/null || {
    echo "google-chrome not found (needed for the headless comparison)" >&2
    exit 1
}

cp "${artifact_dir}/ttr-renderer.js" "${artifact_dir}/ttr-renderer.wasm" \
    "${renderer_dir}/tests/integration/compare.html" "${work}/"

# Build the request list from the fixtures, mirroring capture-goldens.sh.
deno eval '
    const [fixturesDir, widths] = Deno.args;
    const requests = [];
    for (const entry of [...Deno.readDirSync(fixturesDir)].sort((a, b) => a.name < b.name ? -1 : 1)) {
      if (!entry.name.endsWith(".json")) continue;
      const fixture = JSON.parse(Deno.readTextFileSync(`${fixturesDir}/${entry.name}`));
      if (fixture.expected?.mode !== "blocks") continue;
      for (const width of widths.split(",")) {
        requests.push({
          id: `${fixture.id}.w${width}.light`,
          request: {
            schemaVersion: 1,
            content: { mode: "blocks", richMessage: fixture.expected.canonical },
            viewportWidth: Number(width),
            theme: "light",
            scale: 1,
          },
        });
      }
    }
    console.log(JSON.stringify(requests));
' "${renderer_dir}/tests/fixtures" "$(IFS=,; echo "${widths[*]}")" \
    > "${work}/requests.json"

python3 -m http.server "${port}" --directory "${work}" >/dev/null 2>&1 &
server=$!
trap 'kill "${server}" 2>/dev/null || true; rm -rf "${work}"' EXIT
sleep 1

google-chrome --headless=new --no-sandbox --disable-gpu \
    --virtual-time-budget=600000 \
    --dump-dom "http://localhost:${port}/compare.html" 2>/dev/null \
    | python3 -c '
import html, re, sys
page = sys.stdin.read()
match = re.search(r"<pre id=\"out\">(.*?)</pre>", page, re.S)
print(html.unescape(match.group(1)) if match else "{}")
' > "${work}/wasm-results.json"

python3 - "${work}/wasm-results.json" "${goldens}" <<'PY'
import json, struct, sys, zlib

results_path, goldens = sys.argv[1], sys.argv[2]
payload = json.load(open(results_path))
if not payload.get("ok"):
    print("WASM run failed:", payload.get("error", "unknown"))
    raise SystemExit(1)

sys.path.insert(0, goldens + "/../../scripts")
from importlib.machinery import SourceFileLoader
compare = SourceFileLoader("compare", goldens + "/../../scripts/compare-renders.py").load_module()

failures = 0
checked = 0
for entry in payload["results"]:
    golden = f"{goldens}/{entry['id']}.png"
    try:
        width, height, pixels = compare.read_png_rgba(golden)
    except FileNotFoundError:
        print(f"MISSING GOLDEN: {entry['id']}")
        failures += 1
        continue
    checked += 1
    if entry["status"] != "ok":
        print(f"RENDER FAILED: {entry['id']} -> {entry['status']}")
        failures += 1
        continue
    if (width, height) != (entry["width"], entry["height"]):
        print(f"GEOMETRY: {entry['id']} native {width}x{height} "
              f"!= wasm {entry['width']}x{entry['height']}")
        failures += 1
        continue
    checksum = 0x811c9dc5
    for byte in pixels:
        checksum ^= byte
        checksum = (checksum * 0x01000193) & 0xFFFFFFFF
    if checksum != entry["checksum"]:
        print(f"PIXELS: {entry['id']} checksum {checksum} != {entry['checksum']}")
        failures += 1

print(f"compared {checked} renders, {failures} failures")
raise SystemExit(1 if failures else 0)
PY
