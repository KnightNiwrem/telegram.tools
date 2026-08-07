import lock from "../renderer/artifact.lock.json" with { type: "json" };
import { RichMessageEditor } from "../islands/RichMessageEditor.tsx";

export default () => (
  <>
    <header class="mb-5">
      <a
        href="https://telegram.tools"
        class="inline-flex items-center gap-1 text-grammy"
      >
        ← telegram.tools
      </a>
    </header>
    <RichMessageEditor
      assetVersion={lock.tag}
      wasmBytes={lock.files["ttr-renderer.wasm"].bytes}
    />
  </>
);
