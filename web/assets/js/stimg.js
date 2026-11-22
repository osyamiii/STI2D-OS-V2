/* STI2D-OS V2 — éléments originaux : Copyright © 2026 Yamii (cjnw) et Derim (irationnel).
 * Tous droits réservés. Voir /LICENSE et web/LEGAL.html. Les composants tiers gardent
 * leurs licences propres. Contact : https://discord.gg/sti2d
 */
/* STI2D-OS portal — pictures for the calculator ("stimg").
 *
 * The calculator cannot open PNG, JPEG or GIF files, and its file system holds about 42 KB in
 * all. So a picture is turned into a small palette image the firmware reads
 * (firmware/v2/src/stimg.c), wrapped in a Python script of comments that can be sent like any
 * script (my.numworks.com, nwlink):
 *
 *   #STIMG1 <w> <h> <colours>
 *   #P <RGB565 palette, 4 hex digits per colour>
 *   #D <base64 of RLE bytes: (run-1) << 4 | index>       (lines of 76 characters)
 *
 * At most 160 x 120 (shown x2 on the 320 x 240 screen) and 16 colours. A GIF becomes its first
 * frame: an animation would not fit in the calculator's memory, and the firmware has no decoder.
 */
(function () {
  "use strict";
  var MAX_BYTES = 10 * 1024 * 1024;
  var TYPES = { "image/png": "PNG", "image/jpeg": "JPEG", "image/gif": "GIF", "image/webp": "WebP" };
  var W = 160, H = 120;

  function sniff(bytes) {
    var b = bytes;
    if (b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47) return "image/png";
    if (b[0] === 0xff && b[1] === 0xd8 && b[2] === 0xff) return "image/jpeg";
    if (b[0] === 0x47 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x38) return "image/gif";
    if (b[0] === 0x52 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x46 && b[8] === 0x57 && b[9] === 0x45) return "image/webp";
    return null;
  }

  /* -> Promise<{ type, label }> or a rejection with a readable message */
  function check(file) {
    if (!file) return Promise.reject(new Error("Aucun fichier."));
    if (file.size > MAX_BYTES) return Promise.reject(new Error("Image trop lourde (10 Mo au plus)."));
    return file.slice(0, 16).arrayBuffer().then(function (buf) {
      var type = sniff(new Uint8Array(buf));
      if (!type) throw new Error("Format non reconnu : PNG, JPEG, GIF ou WebP uniquement.");
      return { type: type, label: TYPES[type] };
    });
  }

  function decode(file) {
    if (window.createImageBitmap) {
      return createImageBitmap(file).catch(function () {
        return decodeWithImg(file);
      });
    }
    return decodeWithImg(file);
  }

  function decodeWithImg(file) {
    return new Promise(function (resolve, reject) {
      var url = URL.createObjectURL(file), img = new Image();
      img.onload = function () {
        resolve(img);
        setTimeout(function () {
          URL.revokeObjectURL(url);
        }, 1000);
      };
      img.onerror = function () {
        URL.revokeObjectURL(url);
        reject(new Error("Image illisible ou corrompue."));
      };
      img.src = url;
    });
  }

  /* draws the picture into w x h, cropped to fill ("cover") or letterboxed ("contain") */
  function rasterize(src, w, h, fit) {
    var c = document.createElement("canvas");
    c.width = w;
    c.height = h;
    var g = c.getContext("2d");
    g.fillStyle = "#000";
    g.fillRect(0, 0, w, h);
    var sw = src.width, sh = src.height;
    var scale = fit === "contain" ? Math.min(w / sw, h / sh) : Math.max(w / sw, h / sh);
    var dw = sw * scale, dh = sh * scale;
    g.imageSmoothingEnabled = true;
    g.imageSmoothingQuality = "high";
    g.drawImage(src, (w - dw) / 2, (h - dh) / 2, dw, dh);
    return g.getImageData(0, 0, w, h);
  }

  /* median cut on the pixels -> palette of n [r, g, b] */
  function medianCut(data, n) {
    var px = [];
    for (var i = 0; i < data.length; i += 4) px.push([data[i], data[i + 1], data[i + 2]]);
    var boxes = [px];
    while (boxes.length < n) {
      var bi = -1, best = -1, ch = 0;
      boxes.forEach(function (b, k) {
        if (b.length < 2) return;
        for (var c = 0; c < 3; c++) {
          var lo = 255, hi = 0;
          for (var j = 0; j < b.length; j++) {
            var v = b[j][c];
            if (v < lo) lo = v;
            if (v > hi) hi = v;
          }
          var range = (hi - lo) * Math.sqrt(b.length);
          if (range > best) (best = range), (bi = k), (ch = c);
        }
      });
      if (bi < 0 || best <= 0) break;
      var box = boxes[bi].sort(function (a, b) {
        return a[ch] - b[ch];
      });
      var mid = box.length >> 1;
      boxes.splice(bi, 1, box.slice(0, mid), box.slice(mid));
    }
    return boxes.map(function (b) {
      var r = 0, g = 0, bl = 0;
      b.forEach(function (p) {
        r += p[0];
        g += p[1];
        bl += p[2];
      });
      return [Math.round(r / b.length), Math.round(g / b.length), Math.round(bl / b.length)];
    });
  }

  function to565(c) {
    return ((c[0] & 0xf8) << 8) | ((c[1] & 0xfc) << 3) | (c[2] >> 3);
  }
  function from565(v) {
    var r = v >> 11, g = (v >> 5) & 63, b = v & 31;
    return [(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)];
  }

  function nearest(pal, r, g, b) {
    var best = 0, bd = 1e9;
    for (var k = 0; k < pal.length; k++) {
      var dr = r - pal[k][0], dg = g - pal[k][1], db = b - pal[k][2];
      var d = dr * dr * 0.3 + dg * dg * 0.59 + db * db * 0.11;
      if (d < bd) (bd = d), (best = k);
    }
    return best;
  }

  /* -> { indices: Uint8Array(w*h), palette: [[r,g,b]] (already RGB565-exact) } */
  function quantize(img, colors, dither) {
    var pal = medianCut(img.data, colors).map(function (c) {
      return from565(to565(c));
    });
    /* identical entries after RGB565 rounding: keep one */
    var seen = {};
    pal = pal.filter(function (c) {
      var k = to565(c);
      if (seen[k]) return false;
      seen[k] = 1;
      return true;
    });
    var w = img.width, h = img.height, idx = new Uint8Array(w * h);
    var buf = new Float32Array(img.data.length);
    for (var i = 0; i < buf.length; i++) buf[i] = img.data[i];
    for (var y = 0; y < h; y++)
      for (var x = 0; x < w; x++) {
        var o = (y * w + x) * 4;
        var r = Math.max(0, Math.min(255, buf[o])), g = Math.max(0, Math.min(255, buf[o + 1])), b = Math.max(0, Math.min(255, buf[o + 2]));
        var k = nearest(pal, r, g, b);
        idx[y * w + x] = k;
        if (dither) {
          var er = r - pal[k][0], eg = g - pal[k][1], eb = b - pal[k][2];
          var spread = function (dx, dy, f) {
            var xx = x + dx, yy = y + dy;
            if (xx < 0 || xx >= w || yy >= h) return;
            var q = (yy * w + xx) * 4;
            buf[q] += er * f;
            buf[q + 1] += eg * f;
            buf[q + 2] += eb * f;
          };
          spread(1, 0, 7 / 16);
          spread(-1, 1, 3 / 16);
          spread(0, 1, 5 / 16);
          spread(1, 1, 1 / 16);
        }
      }
    return { indices: idx, palette: pal, width: w, height: h };
  }

  function rle(indices) {
    var out = [];
    for (var i = 0; i < indices.length; ) {
      var v = indices[i], run = 1;
      while (run < 16 && i + run < indices.length && indices[i + run] === v) run++;
      out.push(((run - 1) << 4) | v);
      i += run;
    }
    return new Uint8Array(out);
  }

  function base64(bytes) {
    var s = "";
    for (var i = 0; i < bytes.length; i += 0x8000) s += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
    return btoa(s);
  }

  function hex4(v) {
    return ("000" + v.toString(16).toUpperCase()).slice(-4);
  }

  /* the text of stimg_<name>.py (without the record's leading flag byte: the tools add it) */
  function encode(q) {
    var b64 = base64(rle(q.indices)), lines = [];
    for (var i = 0; i < b64.length; i += 76) lines.push("#D " + b64.slice(i, i + 76));
    return (
      "#STIMG1 " + q.width + " " + q.height + " " + q.palette.length + "\n" +
      "#P " + q.palette.map(function (c) { return hex4(to565(c)); }).join("") + "\n" +
      lines.join("\n") + "\n"
    );
  }

  /* the image as the calculator will show it */
  function previewCanvas(q) {
    var c = document.createElement("canvas");
    c.width = q.width;
    c.height = q.height;
    var g = c.getContext("2d"), img = g.createImageData(q.width, q.height);
    for (var i = 0, o = 0; i < q.indices.length; i++, o += 4) {
      var p = q.palette[q.indices[i]];
      img.data[o] = p[0];
      img.data[o + 1] = p[1];
      img.data[o + 2] = p[2];
      img.data[o + 3] = 255;
    }
    g.putImageData(img, 0, 0);
    return c;
  }

  /* "Photo de vacances (1).JPG" -> "photo_de_vacances_1" (the firmware keeps 24 characters for the record name) */
  function slug(name) {
    var s = (name || "image").replace(/\.[^.]+$/, "").normalize("NFD").replace(/[̀-ͯ]/g, "");
    s = s.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
    return (s || "image").slice(0, 14);
  }

  /* file -> Promise<{ text, bytes, filename, canvas, colors, width, height }> */
  function convert(file, opts) {
    opts = opts || {};
    var colors = Math.max(2, Math.min(16, opts.colors || 16));
    var w = opts.small ? 80 : W, h = opts.small ? 60 : H;
    return decode(file).then(function (src) {
      var q = quantize(rasterize(src, w, h, opts.fit), colors, !!opts.dither);
      var text = encode(q);
      return {
        text: text,
        bytes: new Blob([text]).size,
        filename: "stimg_" + slug(opts.name || file.name) + ".py",
        canvas: previewCanvas(q),
        colors: q.palette.length,
        width: w,
        height: h,
      };
    });
  }

  window.STIImage = {
    check: check, convert: convert, decode: decode, slug: slug, MAX_BYTES: MAX_BYTES,
    /* pure parts, used by tests/stimg_crosscheck (Node + the firmware's C decoder) */
    _quantize: quantize, _encode: encode, _to565: to565,
  };
})();
