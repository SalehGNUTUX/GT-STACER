/* GT-STACER website service worker — minimal offline-first cache.
 *
 * Strategy: precache the shell on install, then serve cache-first with a
 * background refresh. Versioned cache key — bumping CACHE_VERSION evicts
 * the previous build on next visit.
 */
const CACHE_VERSION = "gtstacer-v26.06-stable";
const PRECACHE = [
  "./",
  "./index.html",
  "./assets/styles.css",
  "./assets/app.js",
  "./manifest.webmanifest",
  "./images/GT-STACER.png",
  "./images/favicon.ico",
  "./images/laptops.png",
  "./fonts/Ubuntu Arabic Regular.otf",
  "./fonts/Ubuntu Arabic Bold.ttf",
];

self.addEventListener("install", (event) => {
  event.waitUntil(
    caches.open(CACHE_VERSION).then((c) => c.addAll(PRECACHE)).catch(() => {})
  );
  self.skipWaiting();
});

self.addEventListener("activate", (event) => {
  // Drop older versions so the cache doesn't grow without bound.
  event.waitUntil(
    caches.keys().then((keys) =>
      Promise.all(keys.filter((k) => k !== CACHE_VERSION).map((k) => caches.delete(k)))
    )
  );
  self.clients.claim();
});

self.addEventListener("fetch", (event) => {
  const req = event.request;
  if (req.method !== "GET") return;
  // Don't try to cache cross-origin (font-awesome CDN, GitHub raw, etc).
  const url = new URL(req.url);
  if (url.origin !== self.location.origin) return;

  event.respondWith(
    caches.match(req).then((cached) => {
      const fresh = fetch(req)
        .then((resp) => {
          if (resp && resp.ok)
            caches.open(CACHE_VERSION).then((c) => c.put(req, resp.clone()));
          return resp;
        })
        .catch(() => cached);
      return cached || fresh;
    })
  );
});
