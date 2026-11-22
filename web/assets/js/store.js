/* STI2D-OS V2 — éléments originaux : Copyright © 2026 Yamii (cjnw) et Derim (irationnel).
 * Tous droits réservés. Voir /LICENSE et web/LEGAL.html. Les composants tiers gardent
 * leurs licences propres. Contact : https://discord.gg/sti2d
 */
/* STI2D-OS portal — personal storage (IndexedDB).
 *
 * Two object stores in the database "sti2d-os":
 *   items  metadata of an imported file   { id, kind: "nwa" | "wallpaper", name, size, importedAt, ... }
 *   blobs  the file itself                { id, blob }  (and the cover image: id + ":cover")
 * Binary files never go to localStorage (5 MB, strings only). A metadata record and its blob are
 * written in one transaction: an import either fully lands or not at all. Deleting removes both,
 * which gives the space back. The 81 included games are never written here.
 */
(function () {
  "use strict";
  var DB_NAME = "sti2d-os";
  var DB_VERSION = 1;
  var dbPromise = null;

  function open() {
    if (dbPromise) return dbPromise;
    dbPromise = new Promise(function (resolve, reject) {
      if (!("indexedDB" in window)) {
        reject(new StoreError("unsupported", "Ce navigateur ne propose pas IndexedDB : la bibliothèque personnelle est indisponible."));
        return;
      }
      var req;
      try {
        req = indexedDB.open(DB_NAME, DB_VERSION);
      } catch (e) {
        reject(new StoreError("blocked", "Le stockage local est bloqué (navigation privée ou réglages du navigateur)."));
        return;
      }
      req.onupgradeneeded = function () {
        var db = req.result;
        if (!db.objectStoreNames.contains("items")) {
          var items = db.createObjectStore("items", { keyPath: "id" });
          items.createIndex("kind", "kind", { unique: false });
        }
        if (!db.objectStoreNames.contains("blobs")) db.createObjectStore("blobs", { keyPath: "id" });
      };
      req.onsuccess = function () {
        var db = req.result;
        db.onversionchange = function () {
          db.close();
          dbPromise = null;
        };
        resolve(db);
      };
      req.onerror = function () {
        reject(new StoreError("blocked", "Impossible d'ouvrir le stockage local (" + (req.error && req.error.name) + ")."));
      };
      req.onblocked = function () {
        reject(new StoreError("blocked", "Le stockage est utilisé par un autre onglet : ferme-le puis réessaie."));
      };
    });
    return dbPromise;
  }

  function StoreError(code, message) {
    this.code = code;
    this.message = message;
  }
  StoreError.prototype = Object.create(Error.prototype);

  function asStoreError(err) {
    if (err instanceof StoreError) return err;
    var name = err && (err.name || (err.target && err.target.error && err.target.error.name));
    if (name === "QuotaExceededError")
      return new StoreError("quota", "Espace de stockage du navigateur insuffisant. Supprime des fichiers importés ou libère de l'espace.");
    return new StoreError("io", "Erreur du stockage local" + (name ? " (" + name + ")" : "") + ".");
  }

  function tx(stores, mode, work) {
    return open().then(function (db) {
      return new Promise(function (resolve, reject) {
        var t;
        try {
          t = db.transaction(stores, mode);
        } catch (e) {
          reject(asStoreError(e));
          return;
        }
        var result;
        t.oncomplete = function () {
          resolve(result);
        };
        t.onabort = t.onerror = function (e) {
          reject(asStoreError(t.error || e));
        };
        try {
          result = work(t);
        } catch (e) {
          try {
            t.abort();
          } catch (_) {}
          reject(asStoreError(e));
        }
      });
    });
  }

  function reqValue(r, box, key) {
    r.onsuccess = function () {
      box[key] = r.result;
    };
  }

  /* Saves the metadata, the file and its cover (a Blob or null) together. */
  function put(item, blob, cover) {
    return tx(["items", "blobs"], "readwrite", function (t) {
      t.objectStore("items").put(item);
      t.objectStore("blobs").put({ id: item.id, blob: blob });
      if (cover) t.objectStore("blobs").put({ id: item.id + ":cover", blob: cover });
      return item;
    });
  }

  function update(item) {
    return tx(["items"], "readwrite", function (t) {
      t.objectStore("items").put(item);
      return item;
    });
  }

  function setCover(id, cover) {
    return tx(["blobs"], "readwrite", function (t) {
      if (cover) t.objectStore("blobs").put({ id: id + ":cover", blob: cover });
      else t.objectStore("blobs").delete(id + ":cover");
    });
  }

  function list(kind) {
    var box = {};
    return tx(["items"], "readonly", function (t) {
      var s = t.objectStore("items");
      reqValue(kind ? s.index("kind").getAll(kind) : s.getAll(), box, "v");
      return box;
    }).then(function () {
      return (box.v || []).sort(function (a, b) {
        return (b.importedAt || 0) - (a.importedAt || 0);
      });
    });
  }

  function getBlob(id) {
    var box = {};
    return tx(["blobs"], "readonly", function (t) {
      reqValue(t.objectStore("blobs").get(id), box, "v");
    }).then(function () {
      return box.v ? box.v.blob : null;
    });
  }

  function remove(id) {
    return tx(["items", "blobs"], "readwrite", function (t) {
      t.objectStore("items").delete(id);
      t.objectStore("blobs").delete(id);
      t.objectStore("blobs").delete(id + ":cover");
    });
  }

  function clearKind(kind) {
    return list(kind).then(function (items) {
      return Promise.all(
        items.map(function (i) {
          return remove(i.id);
        })
      ).then(function () {
        return items.length;
      });
    });
  }

  /* What the browser reports: used and quota bytes for this site (an estimate), or null. */
  function estimate() {
    if (!navigator.storage || !navigator.storage.estimate) return Promise.resolve(null);
    return navigator.storage.estimate().catch(function () {
      return null;
    });
  }

  /* Asks the browser to keep the data even under storage pressure (it may say no). */
  function persist() {
    if (!navigator.storage || !navigator.storage.persist) return Promise.resolve(false);
    return navigator.storage.persisted().then(function (p) {
      return p || navigator.storage.persist();
    }).catch(function () {
      return false;
    });
  }

  function newId(prefix) {
    var r = "";
    if (window.crypto && crypto.getRandomValues) {
      var a = new Uint32Array(2);
      crypto.getRandomValues(a);
      r = a[0].toString(36) + a[1].toString(36);
    } else r = Math.random().toString(36).slice(2);
    return prefix + "-" + Date.now().toString(36) + "-" + r;
  }

  window.STIStore = {
    put: put,
    update: update,
    setCover: setCover,
    list: list,
    getBlob: getBlob,
    remove: remove,
    clearKind: clearKind,
    estimate: estimate,
    persist: persist,
    newId: newId,
    StoreError: StoreError,
  };
})();
