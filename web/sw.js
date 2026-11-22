/* STI2D-OS V2 — éléments originaux : Copyright © 2026 Yamii (cjnw) et Derim (irationnel).
 * Tous droits réservés. Voir /LICENSE et web/LEGAL.html. Les composants tiers gardent
 * leurs licences propres. Contact : https://discord.gg/sti2d
 */
/* STI2D-OS portal — offline support.
 *
 * Network first for the portal's own files, cache as a fallback: an update of styles.css or
 * app.js is always used when online (the previous worker served the cache first and forever,
 * so a stale stylesheet could stay). Library downloads (.nwa) are not cached: they are big and
 * stay on the server.
 */
var CACHE = "sti2d-portal-2.1.0";
var CORE = [
  "./",
  "./index.html",
  "./assets/styles.css",
  "./assets/js/app.js",
  "./assets/js/store.js",
  "./assets/js/nwa.js",
  "./assets/js/stimg.js",
  "./assets/favicon.svg",
  "./data/games.js",
  "./data/ai.js",
  "./manifest.webmanifest",
];

self.addEventListener("install", function (event) {
  event.waitUntil(
    caches.open(CACHE).then(function (cache) {
      return cache.addAll(CORE);
    }).then(function () {
      return self.skipWaiting();
    })
  );
});

self.addEventListener("activate", function (event) {
  event.waitUntil(
    caches.keys().then(function (keys) {
      return Promise.all(keys.filter(function (k) { return k !== CACHE; }).map(function (k) { return caches.delete(k); }));
    }).then(function () {
      return self.clients.claim();
    })
  );
});

self.addEventListener("fetch", function (event) {
  var req = event.request;
  var url = new URL(req.url);
  if (req.method !== "GET" || url.origin !== self.location.origin) return;
  var scope = new URL(self.registration.scope);
  if (url.pathname.indexOf(scope.pathname) !== 0) return; /* only the portal itself, not ../library */
  event.respondWith(
    fetch(req).then(function (res) {
      if (res.ok && res.type === "basic") {
        var copy = res.clone();
        caches.open(CACHE).then(function (c) { c.put(req, copy); });
      }
      return res;
    }).catch(function () {
      return caches.match(req).then(function (hit) {
        return hit || caches.match("./index.html");
      });
    })
  );
});
