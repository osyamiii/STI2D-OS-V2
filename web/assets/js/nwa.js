/* STI2D-OS V2 — éléments originaux : Copyright © 2026 Yamii (cjnw) et Derim (irationnel).
 * Tous droits réservés. Voir /LICENSE et web/LEGAL.html. Les composants tiers gardent
 * leurs licences propres. Contact : https://discord.gg/sti2d
 */
/* STI2D-OS portal — reading .nwa files in the browser.
 *
 * A .nwa is an EADK app for NumWorks: an ELF 32-bit little-endian ARM *relocatable* object
 * (the installer links it at its place in flash). This checks the format and reads what the file
 * itself says, reliably:
 *   .rodata.eadk_app_name   the name shown on the calculator
 *   .rodata.eadk_api_level  EADK API level (0 today)
 *   .rodata.eadk_app_icon   55 x 56 RGB565 icon, LZ4 block
 *   section sizes           flash (code + constants) and RAM (.data + .bss) the app needs
 *
 * A valid format does not prove the app runs on a given calculator: that depends on the
 * calculator's software (Epsilon >= 23.2, Upsilon...), the RAM it gives apps and the free app
 * space. The checks say which is which.
 */
(function () {
  "use strict";
  var MAX_FILE = 8 * 1024 * 1024; /* the largest app space (Enlarged Extra Apps) is 6.5 MB */
  var APP_SPACE = 2031616;        /* standard app space from 0x90200000 */
  var RAM_EPSILON = 148928;       /* RAM Epsilon 23.2 gives apps */
  var RAM_CUSTOM = 133652;        /* custom builds of Epsilon 25.2 */
  var RAM_UPSILON = 107674;       /* Upsilon */
  var ICON_W = 55, ICON_H = 56;

  function u16(d, o) {
    return d[o] | (d[o + 1] << 8);
  }
  function u32(d, o) {
    return (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;
  }

  function sections(d) {
    var shoff = u32(d, 0x20), shentsize = u16(d, 0x2e), shnum = u16(d, 0x30), shstrndx = u16(d, 0x32);
    if (!shoff || shentsize < 40 || !shnum || shstrndx >= shnum || shoff + shnum * shentsize > d.length) return null;
    var heads = [];
    for (var i = 0; i < shnum; i++) {
      var o = shoff + i * shentsize;
      heads.push({ name: u32(d, o), type: u32(d, o + 4), flags: u32(d, o + 8), offset: u32(d, o + 16), size: u32(d, o + 20) });
    }
    var strtab = heads[shstrndx];
    if (strtab.offset + strtab.size > d.length) return null;
    var out = {};
    heads.forEach(function (h) {
      var s = strtab.offset + h.name, e = s;
      while (e < d.length && d[e] && e - s < 128) e++;
      var name = "";
      for (var k = s; k < e; k++) name += String.fromCharCode(d[k]);
      h.label = name;
      if (h.type !== 8 && h.offset + h.size > d.length) h.broken = true;
      out[name] = h;
    });
    return out;
  }

  function lz4(src, size) {
    var out = new Uint8Array(size), o = 0, i = 0;
    while (i < src.length && o < size) {
      var token = src[i++], n = token >> 4, b;
      if (n === 15) do { b = src[i++]; n += b; } while (b === 255 && i < src.length);
      if (o + n > size || i + n > src.length) throw new Error("lz4");
      out.set(src.subarray(i, i + n), o);
      o += n;
      i += n;
      if (i >= src.length || o >= size) break;
      var off = src[i] | (src[i + 1] << 8);
      i += 2;
      if (!off || off > o) throw new Error("lz4");
      var m = token & 15;
      if (m === 15) do { b = src[i++]; m += b; } while (b === 255 && i < src.length);
      m += 4;
      for (var k = 0; k < m && o < size; k++, o++) out[o] = out[o - off];
    }
    if (o !== size) throw new Error("lz4");
    return out;
  }

  function iconBlob(pix) {
    return new Promise(function (resolve) {
      try {
        var c = document.createElement("canvas");
        c.width = ICON_W;
        c.height = ICON_H;
        var g = c.getContext("2d"), img = g.createImageData(ICON_W, ICON_H);
        for (var p = 0, q = 0; p < pix.length; p += 2, q += 4) {
          var v = pix[p] | (pix[p + 1] << 8), r = v >> 11, gg = (v >> 5) & 63, bb = v & 31;
          img.data[q] = (r << 3) | (r >> 2);
          img.data[q + 1] = (gg << 2) | (gg >> 4);
          img.data[q + 2] = (bb << 3) | (bb >> 2);
          img.data[q + 3] = 255;
        }
        g.putImageData(img, 0, 0);
        c.toBlob(function (b) {
          resolve(b);
        }, "image/png");
      } catch (e) {
        resolve(null);
      }
    });
  }

  function kb(n) {
    return n >= 1048576 ? (n / 1048576).toFixed(2).replace(".", ",") + " Mo" : Math.max(1, Math.round(n / 1024)) + " Ko";
  }

  /* -> Promise<{ ok, checks: [{level: ok|warn|fail, title, detail}], info: {...}, icon: Blob|null }> */
  function inspect(file) {
    var checks = [];
    var add = function (level, title, detail) {
      checks.push({ level: level, title: title, detail: detail || "" });
    };
    var name = file.name || "";
    if (!/\.nwa$/i.test(name)) {
      add("fail", "Extension .nwa absente", "« " + name + " » n'est pas un fichier .nwa.");
      return Promise.resolve({ ok: false, checks: checks, info: {}, icon: null });
    }
    add("ok", "Extension .nwa");
    if (!file.size) {
      add("fail", "Fichier vide");
      return Promise.resolve({ ok: false, checks: checks, info: {}, icon: null });
    }
    if (file.size > MAX_FILE) {
      add("fail", "Fichier trop gros (" + kb(file.size) + ")", "Aucune calculatrice NumWorks n'a autant d'espace pour une app (8 Mo maximum ici).");
      return Promise.resolve({ ok: false, checks: checks, info: {}, icon: null });
    }
    return file.arrayBuffer().then(function (buf) {
      var d = new Uint8Array(buf), info = { size: file.size };
      if (d.length < 52 || d[0] !== 0x7f || d[1] !== 0x45 || d[2] !== 0x4c || d[3] !== 0x46) {
        add("fail", "Ce n'est pas un fichier ELF", "Un .nwa commence par la signature ELF (7F 45 4C 46). Fichier renommé ou corrompu ?");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      if (d[4] !== 1) {
        add("fail", "ELF 64 bits", "Les apps NumWorks sont des ELF 32 bits.");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      if (d[5] !== 1) {
        add("fail", "ELF big-endian", "Les apps NumWorks sont little-endian.");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      var machine = u16(d, 18), type = u16(d, 16), flags = u32(d, 0x24);
      if (machine !== 40) {
        add("fail", "Processeur non ARM (e_machine = " + machine + ")", "Les NumWorks ont un processeur ARM Cortex-M7.");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      add("ok", "ELF ARM 32 bits little-endian");
      if (type === 1) add("ok", "Objet relocalisable", "Le format attendu : l'installateur le place lui-même en mémoire.");
      else if (type === 2) add("warn", "Exécutable déjà lié", "Ce n'est pas le format .nwa habituel (objet relocalisable) : l'installateur risque de le refuser.");
      else {
        add("fail", "Type ELF inattendu (" + type + ")");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      info.hardFloat = !!(flags & 0x400);
      var s = sections(d);
      if (!s) {
        add("fail", "Table des sections illisible", "Le fichier semble tronqué ou corrompu.");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      var broken = Object.keys(s).filter(function (k) {
        return s[k].broken;
      });
      if (broken.length) {
        add("fail", "Sections hors du fichier", "Le fichier est tronqué (" + broken.slice(0, 3).join(", ") + ").");
        return { ok: false, checks: checks, info: info, icon: null };
      }
      var flash = 0, ram = 0;
      Object.keys(s).forEach(function (k) {
        var h = s[k];
        if (h.type === 1 && /^\.(text|rodata|data)/.test(k)) flash += h.size;
        if (h.type === 8 || (h.type === 1 && /^\.data/.test(k))) ram += h.size;
      });
      info.flash = flash;
      info.ram = ram;
      var nameSec = s[".rodata.eadk_app_name"], iconSec = s[".rodata.eadk_app_icon"], apiSec = s[".rodata.eadk_api_level"];
      if (nameSec) {
        var bytes = d.subarray(nameSec.offset, nameSec.offset + nameSec.size), end = bytes.indexOf(0);
        info.appName = new TextDecoder("utf-8").decode(end >= 0 ? bytes.subarray(0, end) : bytes).trim();
      }
      if (apiSec && apiSec.size >= 4) info.apiLevel = u32(d, apiSec.offset);
      if (nameSec && iconSec) add("ok", "App EADK reconnue", "Nom sur la calculatrice : « " + (info.appName || "(vide)") + " ».");
      else add("warn", "Marqueurs EADK absents", "Pas de nom ni d'icône EADK : ce n'est peut-être pas une app NumWorks.");
      if (apiSec) {
        if (info.apiLevel === 0) add("ok", "API EADK niveau 0", "Le niveau attendu par Epsilon et Upsilon actuels.");
        else add("warn", "API EADK niveau " + info.apiLevel, "Niveau inconnu des logiciels actuels de la calculatrice.");
      }
      /* what the calculator offers, as hints: not a guarantee */
      if (flash > APP_SPACE)
        add("warn", "Taille installée ≈ " + kb(flash), "Plus grand que l'espace apps standard (≈ 1,94 Mo) : il faut « Enlarged Extra Apps » (Nwagra).");
      else add("ok", "Taille installée ≈ " + kb(flash), "Tient dans l'espace apps standard (≈ 1,94 Mo), s'il est libre.");
      if (ram > RAM_EPSILON) add("warn", "RAM demandée ≈ " + kb(ram), "Plus que ce qu'Epsilon donne aux apps (" + kb(RAM_EPSILON) + ") : l'app risque de ne pas démarrer.");
      else if (ram > RAM_CUSTOM) add("warn", "RAM demandée ≈ " + kb(ram), "OK sur Epsilon officiel, trop pour certains logiciels modifiés (" + kb(RAM_CUSTOM) + ") et pour Upsilon.");
      else if (ram > RAM_UPSILON) add("ok", "RAM demandée ≈ " + kb(ram), "OK sur Epsilon ; trop pour Upsilon (" + kb(RAM_UPSILON) + ").");
      else add("ok", "RAM demandée ≈ " + kb(ram), "Tient aussi dans la RAM d'Upsilon.");
      var iconPromise = Promise.resolve(null);
      if (iconSec) {
        try {
          var pix = lz4(d.subarray(iconSec.offset, iconSec.offset + iconSec.size), ICON_W * ICON_H * 2);
          iconPromise = iconBlob(pix);
        } catch (e) {
          add("warn", "Icône illisible", "L'icône intégrée n'a pas pu être décodée.");
        }
      }
      return iconPromise.then(function (icon) {
        var failed = checks.some(function (c) {
          return c.level === "fail";
        });
        return { ok: !failed, checks: checks, info: info, icon: icon };
      });
    });
  }

  window.STINwa = { inspect: inspect, sizeLabel: kb, APP_SPACE: APP_SPACE, RAM_EPSILON: RAM_EPSILON, RAM_UPSILON: RAM_UPSILON };
})();
