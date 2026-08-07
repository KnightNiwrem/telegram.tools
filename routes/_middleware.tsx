import { FreshContext } from "$fresh/server.ts";

const hostname = Deno.env.get("__HOSTNAME");
const HEADER = new TextEncoder().encode(
  "<!--- See something wrong? Let us know in our chat: https://t.me/grammyjs --->",
);

export async function handler(req: Request, ctx: FreshContext) {
  const url = new URL(req.url);
  if (hostname && url.hostname != hostname) {
    url.hostname = hostname;
    return Response.redirect(url);
  }
  try {
    const res = await ctx.next();
    // Renderer assets requested under a versioned URL (?v=<lock tag>)
    // only change when the version does, so they can be cached forever.
    if (
      url.pathname.startsWith("/rich-message-renderer/") &&
      url.searchParams.has("v") &&
      res.status == 200
    ) {
      const headers = new Headers(res.headers);
      headers.set("cache-control", "public, max-age=31536000, immutable");
      return new Response(res.body, { status: res.status, headers });
    }
    if (
      res.body != null &&
      res.status == 200 &&
      res.headers.get("content-type")?.includes("text/html")
    ) {
      const reader = res.body.getReader();
      let headerSent = false;
      const rs = new ReadableStream({
        async pull(controller) {
          const result = await reader.read();
          if (result?.done) {
            controller.close();
          } else if (result?.value) {
            if (!headerSent) {
              headerSent = true;
              controller.enqueue(HEADER);
            }
            controller.enqueue(result.value);
          } else if (!result) {
            controller.error();
          }
        },
        async cancel(reason) {
          try {
            await res.body?.cancel(reason);
          } catch {
            //
          }
        },
      });
      return new Response(rs, { headers: res.headers, status: res.status });
    } else {
      return res;
    }
  } catch (err) {
    console.trace(err);
  }
  return Response.redirect(new URL("/", req.url));
}
