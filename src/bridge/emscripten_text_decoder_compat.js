// Emscripten decodes C strings with a Uint8Array view into WebAssembly.Memory.
// Some Edge releases expose growable WASM memory as a resizable ArrayBuffer,
// while TextDecoder.decode() in the same browser still rejects resizable views.
// Keep the workaround module-local and pass TextDecoder an owned, fixed buffer.
var EmscriptenNativeTextDecoder = globalThis.TextDecoder
var TextDecoder = EmscriptenNativeTextDecoder && class extends EmscriptenNativeTextDecoder {
  decode(input, options) {
    if (ArrayBuffer.isView(input)) {
      input = new Uint8Array(input.buffer, input.byteOffset, input.byteLength).slice()
    }
    return super.decode(input, options)
  }
}
