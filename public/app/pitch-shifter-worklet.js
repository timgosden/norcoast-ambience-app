// Granular pitch shifter — two crossfading grains, Hann windowed
// Uses delay-line technique: grains sweep through a circular buffer at a
// different rate than the write head, producing pitch shift without artefacts.
// Hann windows are offset by exactly half a grain so they always sum to 1.
class PitchShifterProcessor extends AudioWorkletProcessor {
  static get parameterDescriptors() {
    return [{ name: 'pitch', defaultValue: 2, minValue: 0.25, maxValue: 4, automationRate: 'k-rate' }];
  }

  constructor() {
    super();
    this._grain = 4096;          // ~93 ms @ 44100 Hz
    this._bufSize = 16384;       // must be power-of-2 (4 × grain)
    this._buf = new Float32Array(this._bufSize);
    this._w = 0;                 // write head
    this._d1 = this._grain;     // grain 1 delay (sweeps grain → 0)
    this._d2 = this._grain * 0.5; // grain 2 delay (offset by half)
  }

  process(inputs, outputs, parameters) {
    const inp = inputs[0];
    const out = outputs[0];
    if (!inp?.[0] || !out?.[0]) return true;

    const pitch = parameters.pitch[0];
    const rate  = pitch - 1;    // > 0 = pitch up, delay decreases each sample
    const N     = inp[0].length;
    const buf   = this._buf;
    const MASK  = this._bufSize - 1; // fast power-of-2 modulo
    const G     = this._grain;

    for (let i = 0; i < N; i++) {
      // Write mono mix into circular buffer
      let x = 0;
      for (let c = 0; c < inp.length; c++) x += inp[c][i] ?? 0;
      buf[this._w & MASK] = x / (inp.length || 1);

      // Advance both grain delay values
      this._d1 -= rate;
      if (this._d1 < 0)  this._d1 += G;
      if (this._d1 >= G) this._d1 -= G;

      this._d2 -= rate;
      if (this._d2 < 0)  this._d2 += G;
      if (this._d2 >= G) this._d2 -= G;

      // Hann window — grains are offset by G/2, so windows always sum to 1
      const w1 = 0.5 - 0.5 * Math.cos(2 * Math.PI * this._d1 / G);
      const w2 = 0.5 - 0.5 * Math.cos(2 * Math.PI * this._d2 / G);

      // Linear-interpolated read from circular buffer
      const rd = (d) => {
        const p  = this._w - d;
        const i0 = ((Math.floor(p) & MASK) + this._bufSize) & MASK;
        const i1 = (i0 + 1) & MASK;
        const f  = p - Math.floor(p);
        return buf[i0] + (buf[i1] - buf[i0]) * f;
      };

      const y = rd(this._d1) * w1 + rd(this._d2) * w2;
      for (let c = 0; c < out.length; c++) if (out[c]) out[c][i] = y;

      this._w++;
    }
    return true; // keep alive
  }
}

registerProcessor('pitch-shifter', PitchShifterProcessor);
