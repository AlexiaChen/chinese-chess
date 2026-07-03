import assert from 'node:assert/strict'
import { readFile } from 'node:fs/promises'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDirectory = dirname(fileURLToPath(import.meta.url))
const repositoryRoot = resolve(scriptDirectory, '..')
const compatibilitySourcePath = resolve(
  repositoryRoot,
  'src/bridge/emscripten_text_decoder_compat.js',
)
const generatedGluePath = process.argv[2]

if (!generatedGluePath) {
  throw new Error('Usage: node scripts/verify_wasm_text_decoder_compat.mjs <generated-glue.js>')
}

const [compatibilitySource, generatedGlue] = await Promise.all([
  readFile(compatibilitySourcePath, 'utf8'),
  readFile(generatedGluePath, 'utf8'),
])

class EdgeLikeTextDecoder {
  decode(input) {
    if (ArrayBuffer.isView(input) && input.buffer.resizable) {
      throw new TypeError('The provided ArrayBuffer value must not be resizable')
    }

    return String.fromCharCode(...input)
  }
}

const resizableBuffer = new ArrayBuffer(4, { maxByteLength: 8 })
assert.equal(
  resizableBuffer.resizable,
  true,
  'Node.js must support resizable ArrayBuffer for the Edge compatibility regression test',
)

const wasmMemoryView = new Uint8Array(resizableBuffer, 1, 2)
wasmMemoryView.set([79, 75])
assert.throws(
  () => new EdgeLikeTextDecoder().decode(wasmMemoryView),
  /must not be resizable/,
  'The test fixture must reproduce the Edge TextDecoder failure',
)

const loadCompatibilityDecoder = new Function(
  'globalThis',
  `${compatibilitySource}\nreturn TextDecoder`,
)
const CompatibilityTextDecoder = loadCompatibilityDecoder({ TextDecoder: EdgeLikeTextDecoder })
assert.equal(
  new CompatibilityTextDecoder().decode(wasmMemoryView),
  'OK',
  'The compatibility decoder must copy a resizable WASM memory view before decoding',
)

const compatibilityMarker = generatedGlue.indexOf('EmscriptenNativeTextDecoder')
const decoderConstruction = generatedGlue.indexOf('new TextDecoder(')
assert.notEqual(
  compatibilityMarker,
  -1,
  'Generated WASM glue is missing the Edge TextDecoder compatibility wrapper',
)
assert.match(
  generatedGlue,
  /ArrayBuffer\.isView\([^)]*\)/,
  'Generated WASM glue is missing the typed-array guard',
)
assert.match(
  generatedGlue,
  /new Uint8Array\([^)]*\.buffer,[^)]*\.byteOffset,[^)]*\.byteLength\)\.slice\(\)/,
  'Generated WASM glue is missing the fixed-buffer copy',
)
assert.ok(
  decoderConstruction === -1 || compatibilityMarker < decoderConstruction,
  'The compatibility wrapper must load before Emscripten constructs TextDecoder',
)

console.log('Verified Edge TextDecoder compatibility in generated WASM glue')
