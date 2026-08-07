/**
 * Validates fixture canonical forms against the actual JSON Schema file
 * (plan Phase 4: "Schema validation fixtures pass in TypeScript and C++").
 * The hand validator in js/canonical enforces limits; this test enforces
 * the declared structural schema, so the two cannot drift silently.
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import { assert } from "$std/assert/assert.ts";
import { Ajv2020 as Ajv } from "https://esm.sh/ajv@8.17.1/dist/2020?target=deno";

const here = new URL(".", import.meta.url);
const schema = JSON.parse(
  Deno.readTextFileSync(
    new URL("../schema/canonical-rich-message.v1.schema.json", here),
  ),
);

// deno-lint-ignore no-explicit-any
const ajv = new (Ajv as any)({ strict: false, allErrors: true });
const validate = ajv.compile(schema);

Deno.test("JSON Schema accepts every blocks-mode fixture canonical", () => {
  const dir = new URL("./fixtures/", here);
  let checked = 0;
  for (const entry of Deno.readDirSync(dir)) {
    if (!entry.name.endsWith(".json")) continue;
    const fixture = JSON.parse(
      Deno.readTextFileSync(new URL(entry.name, dir)),
    );
    if (fixture.expected?.mode !== "blocks") continue;
    if (
      (fixture.expected.diagnostics ?? []).some(
        (d: { severity: string }) => d.severity === "error",
      )
    ) {
      continue; // over-limit fixtures may be structurally valid or not
    }
    const valid = validate(fixture.expected.canonical);
    assert(
      valid,
      `${entry.name}: ${JSON.stringify(validate.errors, null, 2)}`,
    );
    checked++;
  }
  assert(checked > 40, `only ${checked} fixtures checked`);
});

Deno.test("JSON Schema rejects structurally invalid canonicals", () => {
  const invalid = [
    { schemaVersion: 2, blocks: [] },
    { schemaVersion: 1 },
    { schemaVersion: 1, blocks: [{ kind: "nope" }] },
    { schemaVersion: 1, blocks: [{ kind: "heading", text: { text: "x" } }] },
    {
      schemaVersion: 1,
      blocks: [{
        kind: "paragraph",
        text: { text: "x", entities: [{ type: "url", offset: 0, length: 1 }] },
      }],
    },
  ];
  for (const [index, message] of invalid.entries()) {
    assert(
      !validate(message),
      `invalid message ${index} unexpectedly passed schema validation`,
    );
  }
});
