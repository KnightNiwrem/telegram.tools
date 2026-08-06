#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Native render regression gate (plan §8 layer 3): re-renders every
# blocks-mode fixture and compares pixels + geometry against the committed
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
    deno eval '
        const [fixturePath, width] = Deno.args;
        const fixture = JSON.parse(Deno.readTextFileSync(fixturePath));
        if (fixture.expected?.mode !== "blocks") Deno.exit(3);
        console.log(JSON.stringify({
          schemaVersion: 1,
          content: { mode: "blocks", richMessage: fixture.expected.canonical },
          viewportWidth: Number(width),
          theme: "light",
          scale: 1,
        }));
    ' "${renderer_dir}/tests/fixtures/${id}.json" "${width}" \
        > "${request}" 2>/dev/null || continue
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
