// SPDX-License-Identifier: GPL-3.0-or-later
// Builds a SYNTHETIC "OS" .syx (NOPs plus a few planted instructions) to test tools/find-mix-site.ts.
// It contains no Elektron code. Usage: node tests/tool/make-fake-os.ts out.syx
import { writeFileSync } from 'node:fs'
import { ELE3_DEVICES } from '../../../../../../src/engine/elektron/ele3.ts'
import { contentChecksum, encodeSyx } from '../../../../../../src/engine/elektron/sysex.ts'

const device = ELE3_DEVICES.digitone
const image = new Uint8Array(0x1000)
for (let i = 0; i < image.length; i += 2) { image[i] = 0x4e; image[i + 1] = 0x71 }          // NOPs
const put = (off: number, bytes: number[]) => image.set(bytes, off)
put(0x100, [0x28, 0x3c, 0x80, 0x00, 0xea, 0x70])           // move.l #0x8000ea70,d4
put(0x106, [0x4e, 0xb9, 0x40, 0x01, 0x23, 0x45])           // jsr 0x40012345
put(0x200, [0x48, 0x79, 0x80, 0x00, 0xea, 0x70])           // pea 0x8000ea70
put(0x300, [0x28, 0x3c, 0x80, 0x00, 0x12, 0x34])           // a decoy used once: move.l #0x80001234,d4
put(0x400, [0x28, 0x3c, 0x10, 0x00, 0x00, 0x00])           // outside the range: ignored
const section = new Uint8Array(8 + image.length)           // header: length 0xffffffff (not packed), sum 0 -> raw-header
section.set([0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0]); section.set(image, 8)
const tableEnd = 0x20 + 16, container = new Uint8Array(tableEnd + section.length)
container.set([0x45, 0x4c, 0x45, 0x33])                    // 'ELE3'
container.set([0x31, 0x2e, 0x34, 0x34], 0x14)              // version field "1.44"
const view = new DataView(container.buffer)
view.setUint32(0x1c, 1); view.setUint32(0x20, device.mainSection); view.setUint32(0x24, tableEnd)
view.setUint32(0x28, section.length); view.setUint32(0x2c, device.mainLoad)
container.set(section, tableEnd)
const stream = new Uint8Array(8 + container.length)
new DataView(stream.buffer).setUint32(0, container.length); new DataView(stream.buffer).setUint32(4, contentChecksum(container))
stream.set(container, 8)
const framing = (): Uint8Array => { const f = new Uint8Array(16); f.set([0xf0, 0x00, 0x20, 0x3c, device.sysexId, 0, 0, 0, 0x21]); f[15] = 0xf7; return f }
writeFileSync(process.argv[2], encodeSyx(stream, device.sysexId, framing(), framing()))
