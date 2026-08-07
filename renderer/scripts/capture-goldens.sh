#!/usr/bin/env bash
# Part of the telegram.tools rich-message renderer (GPL-3.0, see
# renderer/LICENSE).
#
# Renders every renderable fixture (blocks mode, and html mode through
# TDesktop's import path) through the native host at the canonical
# profile widths and stores PNG + metadata under tests/native-goldens/
# (plan Phase 1). Requires a built ttr_native_host and deno.
set -euo pipefail

renderer_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
host="${renderer_dir}/build/native/ttr_native_host"
out_dir="${renderer_dir}/tests/native-goldens"
widths=(320 480 720)

[[ -x "${host}" ]] || {
    echo "ttr_native_host not built; run scripts/build-native.sh" >&2
    exit 1
}
mkdir -p "${out_dir}"

for fixture in "${renderer_dir}"/tests/fixtures/*.json; do
    id="$(basename "${fixture}" .json)"
    for width in "${widths[@]}"; do
        request="$(mktemp)"
        # Build a RenderRequest from the fixture (shared mapping in
        # scripts/lib/fixture-request.ts).
        deno eval '
            const [libUrl, fixturePath, width] = Deno.args;
            const { goldenRequestFor } = await import(libUrl);
            const fixture = JSON.parse(Deno.readTextFileSync(fixturePath));
            const request = goldenRequestFor(fixture, Number(width));
            if (request === null) Deno.exit(3);
            console.log(JSON.stringify(request));
        ' "file://${renderer_dir}/scripts/lib/fixture-request.ts" \
            "${fixture}" "${width}" > "${request}" 2>/dev/null || {
            rm -f "${request}"
            continue
        }
        "${host}" --request "${request}" \
            --out "${out_dir}/${id}.w${width}.light.png" \
            --meta "${out_dir}/${id}.w${width}.light.json" \
            || echo "FAILED: ${id} @ ${width}" >&2
        rm -f "${request}"
    done
done
echo "goldens written to ${out_dir}"
