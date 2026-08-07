#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Native render regression gate (plan §8 layer 3): re-renders every
# renderable fixture (blocks mode, and html mode through TDesktop's
# import path) and compares pixels + geometry against the committed
# goldens in tests/native-goldens. Any difference fails; diffs are written
# to tests/diffs/ for review. Run in the same environment that captured
# the goldens (see BUILD-STATUS.md for the dev profile).
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
host="${renderer_dir}/build/native/ttr_native_host"
goldens="${renderer_dir}/tests/native-goldens"
work="$(mktemp -d)"
diffs="${renderer_dir}/tests/diffs"
widths=(320 480 720)
failures=0
compared=0

[[ -x "${host}" ]] || {
    echo "ttr_native_host not built; run scripts/build-native.sh" >&2
    exit 1
}
mkdir -p "${diffs}"

for golden in "${goldens}"/*.png; do
    name="$(basename "${golden}" .png)"      # <id>.w<width>.light
    id="${name%%.w*}"
    width="${name#*.w}"; width="${width%%.*}"
    request="${work}/${name}.request.json"
    # A committed golden must be reproducible: a fixture that can no
    # longer produce a request is a failure, not a silent skip.
    if ! deno eval '
        const [libUrl, fixturePath, width] = Deno.args;
        const { goldenRequestFor } = await import(libUrl);
        const fixture = JSON.parse(Deno.readTextFileSync(fixturePath));
        const request = goldenRequestFor(fixture, Number(width));
        if (request === null) Deno.exit(3);
        console.log(JSON.stringify(request));
    ' "file://${renderer_dir}/scripts/lib/fixture-request.ts" \
        "${renderer_dir}/tests/fixtures/${id}.json" "${width}" \
        > "${request}" 2>/dev/null; then
        echo "NO REQUEST FOR GOLDEN: ${name}"
        failures=$((failures + 1))
        continue
    fi
    candidate="${work}/${name}.png"
    if ! "${host}" --request "${request}" --out "${candidate}" 2>/dev/null; then
        echo "RENDER FAILED: ${name}"
        failures=$((failures + 1))
        continue
    fi
    compared=$((compared + 1))
    if ! python3 "${renderer_dir}/scripts/compare-renders.py" \
            "${golden}" "${candidate}" "${diffs}/${name}.diff.png" \
            > "${work}/${name}.cmp" 2>&1; then
        echo "MISMATCH: ${name} — $(head -1 "${work}/${name}.cmp")"
        failures=$((failures + 1))
    else
        rm -f "${diffs}/${name}.diff.png"
    fi
done

rm -rf "${work}"
echo "compared ${compared} renders, ${failures} failures"
[[ "${failures}" -eq 0 ]]
