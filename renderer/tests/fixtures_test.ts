/**
 * Fidelity fixture corpus tests (plan Phase 1).
 *
 * Loads every fixture in `renderer/tests/fixtures/`, runs the grammY
 * adapter over the recorded source form, and asserts the recorded expected
 * mode, canonical form, and diagnostics.
 *
 * Run with: deno test --allow-read renderer/tests/
 *
 * Part of the telegram.tools rich-message renderer (GPL-3.0, see
 * renderer/LICENSE).
 */

import { assert, assertEquals } from "$std/assert/mod.ts";
import { fromFileUrl } from "$std/path/mod.ts";
import type { InputRichMessage } from "https://deno.land/x/grammy_types@v4.0.0/rich.ts";
import { adaptInputRichMessage } from "../js/adapter-grammy/mod.ts";
import {
  type CanonicalRichMessage,
  type RenderDiagnostic,
  validateCanonicalMessage,
} from "../js/canonical/mod.ts";

interface ExpectedDiagnostic {
  severity: "error" | "warning" | "info";
  code: string;
  /** Prefix of the actual diagnostic path; message text is free-form. */
  path?: string;
}

interface Fixture {
  id: string;
  description: string;
  input: InputRichMessage<unknown>;
  expected: {
    mode: "blocks" | "html" | "markdown" | "invalid";
    canonical?: CanonicalRichMessage;
    diagnostics: ExpectedDiagnostic[];
  };
}

const fixturesDir = fromFileUrl(new URL("./fixtures/", import.meta.url));

function loadFixtures(): Fixture[] {
  const fixtures: Fixture[] = [];
  const names = [...Deno.readDirSync(fixturesDir)]
    .filter((entry) => entry.isFile && entry.name.endsWith(".json"))
    .map((entry) => entry.name)
    .sort();
  for (const name of names) {
    const fixture = JSON.parse(
      Deno.readTextFileSync(fixturesDir + name),
    ) as Fixture;
    assertEquals(
      `${fixture.id}.json`,
      name,
      `fixture id ${fixture.id} must match its file name ${name}`,
    );
    fixtures.push(fixture);
  }
  return fixtures;
}

function matches(
  actual: RenderDiagnostic,
  expected: ExpectedDiagnostic,
): boolean {
  return actual.severity === expected.severity &&
    actual.code === expected.code &&
    (expected.path === undefined ||
      (actual.path ?? "").startsWith(expected.path));
}

const fixtures = loadFixtures();

assert(fixtures.length > 0, "fixture corpus must not be empty");

for (const fixture of fixtures) {
  Deno.test(`fixture ${fixture.id}`, () => {
    const result = adaptInputRichMessage(fixture.input);
    assertEquals(
      result.mode,
      fixture.expected.mode,
      `${fixture.id}: adapter mode`,
    );

    if (result.mode === "blocks") {
      assert(
        fixture.expected.canonical !== undefined,
        `${fixture.id}: blocks-mode fixtures must record expected.canonical`,
      );
      // JSON round-trip drops `undefined`-valued properties (e.g. table cell
      // align/valign the adapter copies verbatim), matching what the fixture
      // file can represent.
      assertEquals(
        JSON.parse(JSON.stringify(result.canonical)),
        fixture.expected.canonical,
        `${fixture.id}: canonical form`,
      );
    }

    for (const expected of fixture.expected.diagnostics) {
      assert(
        result.diagnostics.some((actual) => matches(actual, expected)),
        `${fixture.id}: expected diagnostic ${
          JSON.stringify(expected)
        } not found in ${JSON.stringify(result.diagnostics)}`,
      );
    }

    const expectedErrors = fixture.expected.diagnostics.filter(
      (d) => d.severity === "error",
    );
    for (const actual of result.diagnostics) {
      if (actual.severity !== "error") continue;
      assert(
        expectedErrors.some((expected) => matches(actual, expected)),
        `${fixture.id}: unexpected error diagnostic ${JSON.stringify(actual)}`,
      );
    }

    if (result.mode === "blocks" && expectedErrors.length === 0) {
      const validation = validateCanonicalMessage(result.canonical);
      assert(
        validation.valid,
        `${fixture.id}: canonical form must validate cleanly; got ${
          JSON.stringify(validation.diagnostics)
        }`,
      );
    }
  });
}
