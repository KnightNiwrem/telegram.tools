import { useSignal, useSignalEffect } from "@preact/signals";
import { useEffect, useRef } from "preact/hooks";

import {
  adaptInputRichMessage,
  type GrammyAdapterResult,
} from "../renderer/js/adapter-grammy/mod.ts";
import {
  FIDELITY_MODE_LABELS,
  type FidelityMode,
  htmlContent,
  markdownContent,
} from "../renderer/js/adapter-html/mod.ts";
import type { RenderDiagnostic } from "../renderer/js/canonical/mod.ts";
import {
  blitToCanvas,
  RenderCancelledError,
  type RenderContent,
  RendererUnavailableError,
  type RenderResult,
  RichMessageRenderer,
} from "../renderer/js/sdk/mod.ts";

import { Select } from "../components/Select.tsx";

type InputMode = "blocks" | "html" | "markdown";

const EXAMPLE_BLOCKS = JSON.stringify(
  {
    blocks: [
      { type: "heading", size: 2, text: "Quarterly Report" },
      {
        type: "paragraph",
        text: [
          "Numbers are ",
          { type: "bold", text: "up" },
          " and morale is ",
          { type: "spoiler", text: "complicated" },
          ".",
        ],
      },
      {
        type: "list",
        items: [
          { blocks: [{ type: "paragraph", text: "Ship the renderer" }] },
          {
            blocks: [{ type: "paragraph", text: "Verify the goldens" }],
            has_checkbox: true,
          },
        ],
      },
    ],
  },
  null,
  2,
);

const EXAMPLE_HTML = `<h2>Quarterly Report</h2>
<p>Numbers are <b>up</b> and morale is <tg-spoiler>complicated</tg-spoiler>.</p>`;

const EXAMPLE_MARKDOWN = `## Quarterly Report

Numbers are **up** and morale is ||complicated||.`;

// The GPL renderer artifact ships separately from the application bundle
// (renderer/LICENSING.md); these assets exist once scripts/build-wasm.sh
// output is deployed under static/rich-message-renderer/.
const RENDERER_GLUE_URL = "/rich-message-renderer/ttr-renderer.js";

function severityColor(severity: RenderDiagnostic["severity"]): string {
  switch (severity) {
    case "error":
      return "text-red-500";
    case "warning":
      return "text-yellow-500";
    default:
      return "opacity-60";
  }
}

export function RichMessageEditor() {
  const mode = useSignal<InputMode>("blocks");
  const source = useSignal(EXAMPLE_BLOCKS);
  const width = useSignal(480);
  const theme = useSignal<"light" | "dark">("light");
  const diagnostics = useSignal<RenderDiagnostic[]>([]);
  const canonicalJson = useSignal("");
  const fidelityMode = useSignal<FidelityMode>("blocks");
  const rendererStatus = useSignal<"loading" | "ready" | "unavailable">(
    "loading",
  );
  const rendererVersion = useSignal("");
  const renderResult = useSignal<RenderResult | null>(null);

  const renderer = useRef<RichMessageRenderer | null>(null);
  const canvas = useRef<HTMLCanvasElement | null>(null);
  const debounce = useRef<ReturnType<typeof setTimeout> | undefined>(
    undefined,
  );

  useEffect(() => {
    RichMessageRenderer.load({ glueUrl: RENDERER_GLUE_URL })
      .then((loaded) => {
        renderer.current = loaded;
        rendererStatus.value = "ready";
        rendererVersion.value =
          `${loaded.versionInfo.rendererVersion} · TDesktop ${loaded.versionInfo.tdesktopRevision}`;
      })
      .catch((error) => {
        rendererStatus.value = "unavailable";
        if (!(error instanceof RendererUnavailableError)) {
          console.error(error);
        }
      });
    return () => {
      renderer.current?.dispose();
      renderer.current = null;
    };
  }, []);

  useSignalEffect(() => {
    // Track every input signal, then debounce; the SDK additionally applies
    // latest-request-wins across the WASM boundary.
    const currentMode = mode.value;
    const currentSource = source.value;
    const currentWidth = width.value;
    const currentTheme = theme.value;
    clearTimeout(debounce.current);
    debounce.current = setTimeout(
      () => update(currentMode, currentSource, currentWidth, currentTheme),
      150,
    );
  });

  function update(
    currentMode: InputMode,
    currentSource: string,
    currentWidth: number,
    currentTheme: "light" | "dark",
  ) {
    let content: RenderContent | null = null;
    const collected: RenderDiagnostic[] = [];
    canonicalJson.value = "";
    if (currentMode === "blocks") {
      fidelityMode.value = "blocks";
      let parsed: unknown;
      try {
        parsed = JSON.parse(currentSource);
      } catch (error) {
        collected.push({
          severity: "error",
          code: "editor.json",
          message: `input is not valid JSON: ${(error as Error).message}`,
        });
        diagnostics.value = collected;
        return;
      }
      let adapted: GrammyAdapterResult;
      try {
        // deno-lint-ignore no-explicit-any
        adapted = adaptInputRichMessage(parsed as any);
      } catch (error) {
        collected.push({
          severity: "error",
          code: "editor.adapter",
          message: `adapter failed: ${(error as Error).message}`,
        });
        diagnostics.value = collected;
        return;
      }
      collected.push(...adapted.diagnostics);
      if (adapted.mode === "blocks") {
        canonicalJson.value = JSON.stringify(adapted.canonical, null, 2);
        content = { mode: "blocks", richMessage: adapted.canonical };
      } else if (adapted.mode !== "invalid") {
        collected.push({
          severity: "info",
          code: "editor.mode",
          message:
            "html/markdown fields detected; switch the input mode to preview them",
        });
      }
    } else if (currentMode === "html") {
      const prepared = htmlContent(currentSource);
      content = prepared.content;
      fidelityMode.value = prepared.fidelityMode;
    } else {
      const prepared = markdownContent(currentSource);
      content = prepared.content;
      fidelityMode.value = prepared.fidelityMode;
    }
    if (
      content === null ||
      collected.some((diagnostic) => diagnostic.severity === "error")
    ) {
      diagnostics.value = collected;
      return;
    }
    const active = renderer.current;
    if (active === null) {
      diagnostics.value = collected;
      return;
    }
    active
      .render({
        schemaVersion: 1,
        content,
        viewportWidth: currentWidth,
        theme: currentTheme,
        scale: 1,
      })
      .then((result) => {
        renderResult.value = result;
        diagnostics.value = [...collected, ...result.diagnostics];
        if (canvas.current !== null && result.width > 0) {
          blitToCanvas(result, canvas.current);
        }
      })
      .catch((error) => {
        if (error instanceof RenderCancelledError) return;
        diagnostics.value = [...collected, {
          severity: "error",
          code: "renderer.crashed",
          message: String(error),
        }];
      });
  }

  return (
    <div class="flex flex-col w-full px-5 gap-4">
      <div class="flex flex-wrap gap-3 items-center">
        <div class="w-44">
          <Select
            values={["blocks", "html", "markdown"]}
            value={mode.value}
            nameMap={{
              blocks: "grammY blocks (JSON)",
              html: "HTML",
              markdown: "Markdown",
            }}
            onChange={(value) => {
              mode.value = value as InputMode;
              source.value = value === "blocks"
                ? EXAMPLE_BLOCKS
                : value === "html"
                ? EXAMPLE_HTML
                : EXAMPLE_MARKDOWN;
            }}
          />
        </div>
        <div class="w-32">
          <Select
            values={["320", "480", "720"]}
            value={String(width.value)}
            nameMap={{ "320": "320 px", "480": "480 px", "720": "720 px" }}
            onChange={(value) => width.value = Number(value)}
          />
        </div>
        <div class="w-32">
          <Select
            values={["light", "dark"]}
            value={theme.value}
            nameMap={{ light: "Light", dark: "Dark" }}
            onChange={(value) => theme.value = value as "light" | "dark"}
          />
        </div>
      </div>
      <div class="text-xs opacity-50">
        {FIDELITY_MODE_LABELS[fidelityMode.value]}
      </div>
      <div class="grid grid-cols-1 lg:grid-cols-2 gap-4">
        <textarea
          class="w-full h-96 p-3 font-mono text-sm bg-transparent border border-border rounded-lg resize-y"
          spellcheck={false}
          value={source.value}
          onInput={(event) => source.value = event.currentTarget.value}
        />
        <div class="flex flex-col gap-2">
          {rendererStatus.value === "unavailable" && (
            <div class="text-sm border border-border rounded-lg p-4">
              <p class="font-bold">Native preview unavailable</p>
              <p class="opacity-75 mt-1">
                The Telegram Desktop-derived renderer artifact is not deployed
                in this build. Input parsing, translation to the canonical
                structure, and diagnostics below still reflect the real adapter
                pipeline; the pixel preview requires the separately licensed
                WASM artifact (see renderer/README.md in the repository).
              </p>
            </div>
          )}
          {rendererStatus.value === "ready" && (
            <>
              <canvas
                ref={canvas}
                class="border border-border rounded-lg max-w-full"
              />
              <div class="text-xs opacity-50">
                {rendererVersion.value}
                {renderResult.value !== null &&
                  ` · ${renderResult.value.width}×${renderResult.value.height}`}
              </div>
            </>
          )}
        </div>
      </div>
      {diagnostics.value.length > 0 && (
        <div class="border border-border rounded-lg p-3 text-sm font-mono flex flex-col gap-1">
          {diagnostics.value.map((diagnostic) => (
            <div class={severityColor(diagnostic.severity)}>
              [{diagnostic.severity}] {diagnostic.code}
              {diagnostic.path !== undefined && ` at ${diagnostic.path}`}
              {": "}
              {diagnostic.message}
            </div>
          ))}
        </div>
      )}
      {canonicalJson.value !== "" && (
        <details>
          <summary class="cursor-pointer text-sm opacity-75">
            Canonical structure (schema v1)
          </summary>
          <pre class="text-xs font-mono overflow-x-auto p-3 border border-border rounded-lg mt-2">{canonicalJson.value}</pre>
        </details>
      )}
    </div>
  );
}
