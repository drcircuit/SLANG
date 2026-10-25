const { EventEmitter } = require('node:events');
// Native stdout: uint32 LE JSON length, JSON header, optional RGBA bytes.
class Decoder extends EventEmitter {
  constructor() { super(); this.parts = []; this.size = 0; this.header = null; this.jsonSize = null; }
  take(count) {
    const result = Buffer.allocUnsafe(count); let offset = 0;
    while (offset < count) {
      const part = this.parts[0], n = Math.min(part.length, count - offset);
      part.copy(result, offset, 0, n); offset += n;
      if (n === part.length) this.parts.shift(); else this.parts[0] = part.subarray(n);
    }
    this.size -= count; return result;
  }
  push(bytes) {
    this.parts.push(bytes); this.size += bytes.length;
    for (;;) {
      if (this.jsonSize === null) {
        if (this.size < 4) return;
        this.jsonSize = this.take(4).readUInt32LE();
        if (!this.jsonSize || this.jsonSize > 1048576) throw new Error('Invalid runner header size');
      }
      if (!this.header) {
        if (this.size < this.jsonSize) return;
        this.header = JSON.parse(this.take(this.jsonSize).toString('utf8'));
        const n = this.header.binaryBytes ?? 0;
        if (!Number.isSafeInteger(n) || n < 0 || n > 67108864) throw new Error('Invalid runner payload size');
        if (this.header.type === 'frame' && (!Number.isInteger(this.header.width) || !Number.isInteger(this.header.height) || this.header.width < 1 || this.header.height < 1 || n !== this.header.width * this.header.height * 4)) throw new Error('Invalid canvas frame');
      }
      const count = this.header.binaryBytes ?? 0;
      if (this.size < count) return;
      const event = this.header;
      if (count) event.pixels = this.take(count);
      this.header = null; this.jsonSize = null; this.emit('packet', event);
    }
  }
  end() { if (this.size || this.header || this.jsonSize !== null) throw new Error('Truncated runner output'); }
}
module.exports = { Decoder };
