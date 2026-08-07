importScripts(
  "https://storage.googleapis.com/workbox-cdn/releases/7.0.0/workbox-sw.js",
);

workbox.routing.registerRoute(
  ({ url }) =>
    url.origin == self.location.origin && url.pathname.startsWith("/fonts/"),
  new workbox.strategies.CacheFirst(),
);
