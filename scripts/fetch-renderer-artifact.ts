// Downloads the prebuilt rich-message renderer artifact pinned by
// renderer/artifact.lock.json into static/rich-message-renderer/ and
// verifies each file's sha256 against the lock. Runs as the first half
// of `deno task build`, so a deployment serves exactly the bytes the
// renderer-wasm workflow built and verified against the native goldens
// (the wasm cannot be built in a deploy environment — it needs emsdk,
// Qt for WebAssembly, and ~20 minutes of C++ compilation; see
// renderer/BUILD-STATUS.md).
//
// Idempotent: files already present with matching hashes are kept.

interface ArtifactLock {
  repo: string;
  tag: string;
  files: Record<string, { sha256: string; bytes: number }>;
}

const root = new URL("..", import.meta.url);
const lockUrl = new URL("renderer/artifact.lock.json", root);
const outDir = new URL("static/rich-message-renderer/", root);

let lockText: string;
try {
  lockText = await Deno.readTextFile(lockUrl);
} catch {
  console.error(
    "renderer/artifact.lock.json not found — no renderer artifact has been " +
      "published yet. The renderer-wasm workflow publishes a release and " +
      "commits the lock on its first successful run.",
  );
  Deno.exit(1);
}
const lock: ArtifactLock = JSON.parse(lockText);

async function sha256(bytes: Uint8Array<ArrayBuffer>): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", bytes);
  return [...new Uint8Array(digest)]
    .map((byte) => byte.toString(16).padStart(2, "0"))
    .join("");
}

await Deno.mkdir(outDir, { recursive: true });

for (const [name, expected] of Object.entries(lock.files)) {
  const target = new URL(name, outDir);
  try {
    if (await sha256(await Deno.readFile(target)) === expected.sha256) {
      console.log(`${name}: up to date (${expected.sha256.slice(0, 12)}…)`);
      continue;
    }
  } catch {
    // Missing or unreadable: download below.
  }
  const url =
    `https://github.com/${lock.repo}/releases/download/${lock.tag}/${name}`;
  console.log(`${name}: fetching ${url}`);
  const response = await fetch(url);
  if (!response.ok) {
    console.error(`${name}: HTTP ${response.status} fetching ${url}`);
    Deno.exit(1);
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  const actual = await sha256(bytes);
  if (actual !== expected.sha256) {
    console.error(
      `${name}: sha256 mismatch\n  lock:       ${expected.sha256}\n` +
        `  downloaded: ${actual}`,
    );
    Deno.exit(1);
  }
  if (bytes.length !== expected.bytes) {
    console.error(
      `${name}: size mismatch — lock says ${expected.bytes} bytes, ` +
        `downloaded ${bytes.length}`,
    );
    Deno.exit(1);
  }
  await Deno.writeFile(target, bytes);
  console.log(`${name}: ${bytes.length} bytes, sha256 verified`);
}
