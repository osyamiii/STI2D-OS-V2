// Encodes synthetic pictures with the portal's own encoder (web/assets/js/stimg.js) and writes,
// for each, the stimg_*.py text and the RGB565 colour of every pixel as the portal shows it.
// decode.c then decodes the text with the firmware's decoder (firmware/v2/src/stimg.c) and
// compares pixel by pixel.   node encode.js OUT_DIR
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const out = process.argv[2];
const src = fs.readFileSync(path.join(__dirname, "../../web/assets/js/stimg.js"), "utf8");
const ctx = { window: {}, btoa: (s) => Buffer.from(s, "binary").toString("base64"), Blob: class {}, document: {} };
vm.createContext(ctx);
vm.runInContext(src, ctx);
const S = ctx.window.STIImage;

function picture(w, h, kind) {
  const data = new Uint8ClampedArray(w * h * 4);
  for (let y = 0; y < h; y++)
    for (let x = 0; x < w; x++) {
      const o = (y * w + x) * 4;
      let r, g, b;
      if (kind === "gradient") (r = (x * 255) / w), (g = (y * 255) / h), (b = 128 + 100 * Math.sin(x / 9));
      else if (kind === "flat") (r = x < w / 2 ? 255 : 0), (g = y < h / 2 ? 164 : 20), (b = 43);
      else (r = (x * 37 + y * 11) % 256), (g = (x * x + y) % 256), (b = (y * 13) % 256); /* noise: long, short runs */
      data[o] = r, data[o + 1] = g, data[o + 2] = b, data[o + 3] = 255;
    }
  return { data, width: w, height: h };
}

const cases = [
  ["gradient16", 160, 120, "gradient", 16, false],
  ["gradient_dither", 160, 120, "gradient", 12, true],
  ["flat2", 80, 60, "flat", 4, false],
  ["noise16", 160, 120, "noise", 16, true],
];
for (const [name, w, h, kind, colors, dither] of cases) {
  const q = S._quantize(picture(w, h, kind), colors, dither);
  const text = S._encode(q);
  fs.writeFileSync(path.join(out, `stimg_${name}.py`), text);
  const px = Buffer.alloc(w * h * 2);
  for (let i = 0; i < q.indices.length; i++) px.writeUInt16LE(S._to565(q.palette[q.indices[i]]), i * 2);
  fs.writeFileSync(path.join(out, `stimg_${name}.expect`), px);
  console.log(`${name}: ${w}x${h}, ${q.palette.length} colours, ${text.length} bytes`);
}
