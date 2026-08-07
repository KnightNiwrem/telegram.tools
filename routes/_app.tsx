// The service-worker registration and the metrics snippet are inline
// scripts, which JSX can only express through dangerouslySetInnerHTML.
// deno-lint-ignore-file react-no-danger
import { type PageProps } from "$fresh/server.ts";
import { CookieNotice } from "../islands/CookieNotice.tsx";

const metricsSnippet = Deno.env.get("METRICS_SNIPPET");

export default function App({ Component }: PageProps) {
  return (
    <html>
      <head>
        <script
          dangerouslySetInnerHTML={{
            __html: 'navigator.serviceWorker.register("/sw.js");',
          }}
        />
        <meta charset="utf-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Rich Message Preview</title>
        <meta
          name="description"
          content="Preview how Telegram renders rich messages, right in your browser."
        />
        <link rel="stylesheet" href="/fonts.css" />
        <link rel="stylesheet" href="/main.css" />
        {metricsSnippet
          ? <script dangerouslySetInnerHTML={{ __html: metricsSnippet }} />
          : null}
      </head>
      <body class="font-inter bg-background text-foreground select-none p-5">
        <main class="mx-auto w-full max-w-[900px] flex flex-col">
          <Component />
        </main>
        {metricsSnippet && <CookieNotice />}
      </body>
    </html>
  );
}
