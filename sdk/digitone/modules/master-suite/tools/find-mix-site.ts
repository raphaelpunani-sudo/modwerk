// SPDX-License-Identifier: GPL-3.0-or-later
// Master Suite helper: finds candidate places in YOUR Digitone OS where the master mix buffer is referenced.
//
//   node tools/find-mix-site.ts /path/to/your-digitone-os.syx [--out report.json] [--range 0x80000000:0x8000ffff] [--top 30]
//
// It runs on your computer only and reads the OS file you give it. It uses this repository's own container
// reader and ColdFire decoder (no npm packages needed, Node 22.18+ or 24).
// It prints ADDRESSES, COUNTS, LENGTHS and SHA-256 HASHES only: no OS bytes, no instructions, no code. The report
// is meant to be pasted back into the chat. Never commit or share the OS file itself.
//
// What it looks for: places that load a constant data address in the 0x8000xxxx range with `move.l #addr,Dn`,
// `movea.l #addr,An`, `lea addr.l,An` or `pea addr.l`. The Digitakt's Digi EQ patches exactly such an
// instruction ("move.l #0x8000ea70,d4") where the master block is handed to the outputs. This lists the Digitone's
// candidates, ranked by how many places use the same address. It does not decide which one is the master mix.
import { readFileSync, writeFileSync } from 'node:fs'
import { ELE3_DEVICES, mainImage, readEle3Syx } from '../../../../../src/engine/elektron/ele3.ts'
import { decodeColdFire, readerAt, wholeInstructions } from '../../../../../src/engine/elektron/coldfire-isa.ts'
import { sha256Hex } from '../../../../../src/engine/elektron/hash.ts'

const args = process.argv.slice(2)
const file = args.find(a => !a.startsWith('--'))
const opt = (name: string, fallback: string) => { const i = args.indexOf(name); return i >= 0 && args[i + 1] ? args[i + 1] : fallback }
if (!file) { console.error('Usage: node tools/find-mix-site.ts <digitone-os.syx> [--out report.json] [--range lo:hi] [--top N]'); process.exit(2) }
const [lo, hi] = opt('--range', '0x80000000:0x8000ffff').split(':').map(Number)
const top = Number(opt('--top', '30')), outPath = opt('--out', 'master-suite-sites.json')

const raw = new Uint8Array(readFileSync(file))
const device = ELE3_DEVICES.digitone
const syx = readEle3Syx(raw)
if (syx.deviceId !== device.sysexId) {
  console.error('This file is not a Digitone mk1 OS (device id 0x' + syx.deviceId.toString(16) + ', expected 0x' + device.sysexId.toString(16) + ').')
  process.exit(1)
}
const image = mainImage(syx, device), base = device.mainLoad
const hex = (n: number) => '0x' + n.toString(16).padStart(8, '0')
const u32 = (o: number) => ((image[o] << 24) | (image[o + 1] << 16) | (image[o + 2] << 8) | image[o + 3]) >>> 0

type Hit = { site: number; form: string; reg: string; sha256: string; calls: number[] }
const byAddress = new Map<number, Hit[]>()
const forms: [number, number, string, (w: number) => string][] = [
  [0xf1ff, 0x203c, 'move.l #imm,Dn', w => 'D' + ((w >> 9) & 7)],
  [0xf1ff, 0x207c, 'movea.l #imm,An', w => 'A' + ((w >> 9) & 7)],
  [0xf1ff, 0x41f9, 'lea abs.l,An', w => 'A' + ((w >> 9) & 7)],
  [0xffff, 0x4879, 'pea abs.l', () => '-'],
]
for (let off = 0; off + 6 <= image.length; off += 2) {
  const w = (image[off] << 8) | image[off + 1]
  for (const [mask, value, form, reg] of forms) {
    if ((w & mask) !== value) continue
    const imm = u32(off + 2)
    if (imm < lo || imm > hi) continue
    const site = base + off
    if (!wholeInstructions(image, base, site, 6).ok) continue
    // The call targets in the next 48 bytes: an address list, nothing else.
    const calls: number[] = []
    const read = readerAt(image, base)
    for (let at = site + 6; at < site + 54 && at - base + 2 < image.length;) {
      const insn = decodeColdFire(read(at), at)
      if ((insn.flow === 'jsr' || insn.flow === 'bsr') && insn.target !== undefined) calls.push(insn.target >>> 0)
      if (insn.flow === 'rts' || insn.flow === 'rte' || insn.flow === 'jmp' || insn.flow === 'bra') break
      at += insn.length
    }
    const hit: Hit = { site, form, reg: reg(w), sha256: '', calls }
    hit.sha256 = await sha256Hex(image.slice(off, off + 6))
    if (!byAddress.has(imm)) byAddress.set(imm, [])
    byAddress.get(imm)!.push(hit)
  }
}

const ranked = [...byAddress.entries()].sort((a, b) => b[1].length - a[1].length || a[0] - b[0])
const report = {
  note: 'Addresses, counts, lengths and SHA-256 hashes only. No OS bytes. Safe to paste into the chat.',
  osVersion: syx.version, fileSha256: await sha256Hex(raw), mainImageBytes: image.length, mainImageSha256: await sha256Hex(image),
  loadAddress: hex(base), range: [hex(lo), hex(hi)], candidatesTotal: ranked.length,
  candidates: ranked.slice(0, top).map(([address, hits]) => ({
    address: hex(address), references: hits.length,
    sites: hits.slice(0, 12).map(h => ({ site: hex(h.site), form: h.form, reg: h.reg, stockSha256: h.sha256, callTargetsNext48Bytes: h.calls.map(hex) })),
  })),
}
writeFileSync(outPath, JSON.stringify(report, null, 2) + '\n')
console.log('OS ' + report.osVersion + ', main image ' + image.length + ' bytes loaded at ' + hex(base))
console.log(ranked.length + ' data addresses in ' + hex(lo) + '..' + hex(hi) + ' are loaded by an instruction; top ' + Math.min(top, ranked.length) + ':')
for (const c of report.candidates) console.log('  ' + c.address + '  ' + String(c.references).padStart(3) + ' refs  first site ' + c.sites[0].site + ' (' + c.sites[0].form + ' ' + c.sites[0].reg + ')')
console.log('Full report written to ' + outPath + '. Paste its contents into the chat (it contains no OS bytes).')
