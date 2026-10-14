// host.js - the thin browser layer around the C++ game.
// It loads skyward.wasm, forwards input, and draws what the game hands back with WebGL2:
// a painted ground pass, one batched sprite pass, a light map, glow, bloom and a final grade.
'use strict';

(async function () {
  const FI = {
    CAM_X: 0, CAM_Y: 1, ZOOM: 2, TIME: 3, AMB_R: 4, AMB_G: 5, AMB_B: 6, EXPOSURE: 7,
    MAP_W: 8, MAP_H: 9, TILE: 10, MAP_VERSION: 11, ATLAS_VERSION: 12, LIT_QUADS: 13, ALL_QUADS: 14, LIGHTS: 15,
    FADE: 16, BLOOM: 17, VIGNETTE: 18, SATURATION: 19, GRADE_R: 20, GRADE_G: 21, GRADE_B: 22, REGION: 23,
    SHAKE_X: 24, SHAKE_Y: 25, WIND: 26, COUNT: 27,
  };
  const BTN = { UP: 1, DOWN: 2, LEFT: 4, RIGHT: 8, DASH: 16, FIRE: 32, INTERACT: 64, PAUSE: 128 };
  const MAX_QUADS = 20000;
  const MAX_LIGHTS = 256;
  const ui = window.SkyUI || { onEvent() {}, frame() {}, fail() {} };

  const canvas = document.getElementById('view');
  const gl = canvas.getContext('webgl2', { antialias: false, alpha: false, depth: false, stencil: false, premultipliedAlpha: false, powerPreference: 'high-performance' });
  if (!gl) { ui.fail('This game needs WebGL 2, which this browser does not support. Try a recent Chrome, Edge, Firefox or Safari.'); return; }
  const hdr = !!gl.getExtension('EXT_color_buffer_float');
  const aniso = gl.getExtension('EXT_texture_filter_anisotropic');

  // ---------------------------------------------------------------- the C++ game
  let bytes;
  if (window.SKY_WASM) bytes = Uint8Array.from(atob(window.SKY_WASM), (c) => c.charCodeAt(0));
  else bytes = await (await fetch('skyward.wasm')).arrayBuffer();
  let memory = null;
  const dec = new TextDecoder();
  const str = (p, l) => dec.decode(new Uint8Array(memory.buffer, p, l));
  const { instance } = await WebAssembly.instantiate(bytes, {
    env: {
      js_log: (p, l) => console.log('[skyward]', str(p, l)),
      js_event: (k, p, l, a, b) => ui.onEvent(k, str(p, l), a, b),
      js_llm_ask: (id, p, l) => askModel(id, str(p, l)),
    },
  });
  const X = instance.exports;
  memory = X.memory;

  // the live model answers asynchronously; its reply goes back into the game's text buffer
  const textEnc = new TextEncoder();
  function askModel(id, prompt) {
    const L = window.SkyLLM;
    if (!L || L.state !== 'ready') { setTimeout(() => X.game_llm_fail(id), 0); return; }
    const timeout = new Promise((_, reject) => setTimeout(() => reject(new Error('timed out')), 18000));
    Promise.race([L.ask(prompt), timeout]).then((text) => {
      const bytes = textEnc.encode(String(text)).slice(0, 511);
      new Uint8Array(memory.buffer, X.game_text_buf(), 512).set(bytes);
      X.game_llm_reply(id, bytes.length);
    }).catch(() => X.game_llm_fail(id));
  }
  if (window.SkyLLM) window.SkyLLM.on((state) => X.game_llm_status(state === 'ready' ? 1 : 0));

  // ---------------------------------------------------------------- sound
  // The C++ synth renders blocks of samples; we queue them back to back on the audio clock.
  let actx = null, master = null, nextTime = 0;
  const CHUNK = 2048;
  function startAudio() {
    if (actx) { if (actx.state === 'suspended') actx.resume(); return; }
    const AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return;
    try { actx = new AC({ latencyHint: 'interactive' }); } catch (e) { return; }
    master = actx.createGain();
    master.connect(actx.destination);
    X.game_audio_init(actx.sampleRate);
    nextTime = actx.currentTime + 0.06;
  }
  function pumpAudio() {
    if (!actx || actx.state !== 'running') return;
    if (nextTime < actx.currentTime) nextTime = actx.currentTime + 0.03;
    let guard = 0;
    while (nextTime < actx.currentTime + 0.22 && guard++ < 8) {
      const p = X.game_audio_render(CHUNK);
      const data = new Float32Array(memory.buffer, p, CHUNK * 2);
      const buf = actx.createBuffer(2, CHUNK, actx.sampleRate);
      const L = buf.getChannelData(0), R = buf.getChannelData(1);
      for (let i = 0; i < CHUNK; i++) { L[i] = data[i * 2]; R[i] = data[i * 2 + 1]; }
      const src = actx.createBufferSource();
      src.buffer = buf;
      src.connect(master);
      src.start(nextTime);
      nextTime += CHUNK / actx.sampleRate;
    }
  }
  ['pointerdown', 'keydown', 'touchstart'].forEach((ev) => window.addEventListener(ev, startAudio, { passive: true }));
  document.addEventListener('visibilitychange', () => {
    if (!actx) return;
    if (document.hidden) actx.suspend(); else { actx.resume(); nextTime = 0; }
  });
  window.SkyAudio = {
    volume(music, effects) { X.game_audio_volume(music, effects); },
    click() { X.game_sfx(18); },
  };
  X.__wasm_call_ctors();
  const t0 = performance.now();
  X.game_init((Math.random() * 0xffffffff) >>> 0);
  const bakeMs = performance.now() - t0;

  const ptr = {
    input: X.game_input(), info: X.game_info(), debug: X.game_debug(), palette: X.game_palette(), speakers: X.game_speakers(),
    verts: X.game_verts(), lights: X.game_lights(), atlas: X.game_atlas(),
  };
  const atlasSize = X.game_atlas_size();
  const f32 = () => new Float32Array(memory.buffer);
  const u32 = () => new Uint32Array(memory.buffer);

  // ---------------------------------------------------------------- GL helpers
  function shader(type, src) {
    const s = gl.createShader(type);
    gl.shaderSource(s, src);
    gl.compileShader(s);
    if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(s) + '\n' + src);
    return s;
  }
  function program(vs, fs) {
    const p = gl.createProgram();
    gl.attachShader(p, shader(gl.VERTEX_SHADER, vs));
    gl.attachShader(p, shader(gl.FRAGMENT_SHADER, fs));
    gl.linkProgram(p);
    if (!gl.getProgramParameter(p, gl.LINK_STATUS)) throw new Error(gl.getProgramInfoLog(p));
    const u = {};
    const n = gl.getProgramParameter(p, gl.ACTIVE_UNIFORMS);
    for (let i = 0; i < n; i++) { const info = gl.getActiveUniform(p, i); u[info.name] = gl.getUniformLocation(p, info.name); }
    return { p, u };
  }
  function texture(w, h, fmt, filter) {
    const t = gl.createTexture();
    gl.bindTexture(gl.TEXTURE_2D, t);
    const internal = fmt === 'hdr' ? gl.RGBA16F : gl.RGBA8;
    const type = fmt === 'hdr' ? gl.HALF_FLOAT : gl.UNSIGNED_BYTE;
    gl.texImage2D(gl.TEXTURE_2D, 0, internal, w, h, 0, gl.RGBA, type, null);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, filter || gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, filter || gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    return t;
  }
  function target(w, h, fmt) {
    const tex = texture(w, h, fmt);
    const fb = gl.createFramebuffer();
    gl.bindFramebuffer(gl.FRAMEBUFFER, fb);
    gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, tex, 0);
    return { tex, fb, w, h };
  }
  function free(t) { if (t) { gl.deleteTexture(t.tex); gl.deleteFramebuffer(t.fb); } }

  // ---------------------------------------------------------------- shaders
  const NOISE = `
  float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
  float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), u.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), u.x), u.y);
  }
  float fbm(vec2 p) { float s = 0.0, a = 0.5; for (int i = 0; i < 4; i++) { s += noise(p) * a; p = p * 2.03 + vec2(17.1, -9.7); a *= 0.5; } return s / 0.9375; }
  vec2 worldAt(vec2 fc) { return uCam + vec2(fc.x - uView.x * 0.5, uView.y * 0.5 - fc.y) / uZoom; }`;

  const FULL_VS = `#version 300 es
  out vec2 vUV;
  void main() { vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); vUV = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }`;

  const SPRITE_VS = `#version 300 es
  layout(location = 0) in vec2 aPos; layout(location = 1) in vec2 aUV; layout(location = 2) in vec4 aColor; layout(location = 3) in vec4 aFx;
  uniform vec2 uCam; uniform float uZoom; uniform vec2 uView; uniform float uTime; uniform float uWind;
  out vec2 vUV; out vec4 vColor; out vec2 vFx;
  void main() {
    vec2 p = aPos;
    float s = aFx.x;
    if (s > 0.0) {
      float w = sin(uTime * 1.7 + aPos.x * 0.031 + aPos.y * 0.017) * 0.55 + sin(uTime * 0.63 + aPos.x * 0.0071 - aPos.y * 0.004) * 0.7 + uWind;
      p.x += s * 7.0 * w;
      p.y += s * 0.8 * abs(w);
    }
    vec2 v = (p - uCam) * uZoom;
    gl_Position = vec4(v.x * 2.0 / uView.x, -v.y * 2.0 / uView.y, 0.0, 1.0);
    vUV = aUV; vColor = aColor; vFx = aFx.yz;
  }`;
  const SPRITE_FS = `#version 300 es
  precision mediump float;
  uniform sampler2D uTex;
  in vec2 vUV; in vec4 vColor; in vec2 vFx;
  out vec4 o;
  void main() {
    vec4 t = texture(uTex, vUV);
    vec4 c = vec4(t.rgb * vColor.rgb, t.a) * vColor.a;
    c.rgb = mix(c.rgb, vec3(c.a), vFx.y);
    c.a *= 1.0 - vFx.x;
    o = c;
  }`;

  const GROUND_FS = `#version 300 es
  precision highp float;
  uniform sampler2D uMap; uniform vec2 uMapSize; uniform float uTile;
  uniform vec2 uCam; uniform float uZoom; uniform vec2 uView; uniform float uTime;
  uniform vec3 uPal[11]; uniform float uLiquid; uniform float uRegion;
  in vec2 vUV; out vec4 o;
  ${NOISE}
  // palette slots, filled from the land's definition in C++
  #define GRASS_A uPal[0]
  #define GRASS_B uPal[1]
  #define GRASS_C uPal[2]
  #define FLOOR uPal[3]
  #define DIRT_A uPal[4]
  #define DIRT_B uPal[5]
  #define SHORE uPal[6]
  #define SHALLOW uPal[7]
  #define DEEP uPal[8]
  #define FOAM uPal[9]
  #define GLOW uPal[10]
  vec2 hash2(vec2 p) { return vec2(hash(p), hash(p + 19.19)); }
  // distance to the nearest cell centre, and how far from the edge between cells
  vec2 voronoi(vec2 x) {
    vec2 n = floor(x), f = fract(x);
    float f1 = 8.0, f2 = 8.0;
    for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) {
      vec2 g = vec2(float(i), float(j));
      vec2 r = g + hash2(n + g) - f;
      float d = dot(r, r);
      if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) { f2 = d; }
    }
    return vec2(sqrt(f1), sqrt(f2) - sqrt(f1));
  }
  float mapLiquid(vec2 w) { return texture(uMap, w / (uMapSize * uTile)).g; }
  void main() {
    vec2 world = worldAt(gl_FragCoord.xy);
    vec4 m = texture(uMap, world / (uMapSize * uTile));
    int R = int(uRegion + 0.5), LQ = int(uLiquid + 0.5);
    float n1 = fbm(world * 0.011), n2 = fbm(world * 0.045 + 7.0), n3 = noise(world * 0.33);
    float em = 0.0;

    // ground: broad colour drift, lighter patches, fine detail
    vec3 grass = mix(GRASS_B, GRASS_A, smoothstep(0.3, 0.72, n1));
    grass = mix(grass, GRASS_C, smoothstep(0.58, 0.85, n2) * 0.45);
    float blades = noise(vec2(world.x * 0.55, world.y * 0.16));
    grass *= 0.91 + n3 * 0.08 + blades * 0.07;
    if (R == 2 || R == 5) {
      // bare rock: cracks, and in the crags a faint heat glowing up through them
      float c1 = 1.0 - smoothstep(0.0, 0.026, abs(noise(world * 0.028) - 0.5));
      float c2 = 1.0 - smoothstep(0.0, 0.05, abs(noise(world * 0.07 + 3.0) - 0.5));
      grass *= 1.0 - c1 * 0.38 - c2 * 0.14;
      if (R == 2) {
        float heat = smoothstep(0.45, 0.75, fbm(world * 0.006 + vec2(uTime * 0.02, 0.0)));
        grass += GLOW * c1 * heat * 0.55;
        em = max(em, c1 * heat * 0.5);
      }
    }
    if (R == 3) grass = mix(grass, grass * vec3(0.78, 1.06, 0.86), smoothstep(0.5, 0.72, noise(world * 0.02)) * 0.7);
    if (R == 1) grass = mix(grass, grass * vec3(0.86, 0.9, 0.78), smoothstep(0.45, 0.8, fbm(world * 0.02 + 11.0)) * 0.6);
    grass = mix(grass, grass * vec3(0.85, 0.98, 0.83), m.a * 0.7);
    float forest = m.g < 0.3 ? m.b : 0.0;
    vec3 floorc = FLOOR * (0.88 + noise(world * 0.08) * 0.24);
    grass = mix(grass, floorc, smoothstep(0.2, 0.9, forest) * 0.85);

    // the path: earth with soft ragged edges; worked stone in the high and cursed lands
    float pw = m.r + (fbm(world * 0.07) - 0.5) * 0.32;
    float path = smoothstep(0.43, 0.52, pw);
    vec3 dirt = mix(DIRT_A, DIRT_B, fbm(world * 0.05 + 3.0));
    dirt *= 0.94 + noise(world * 0.45) * 0.1;
    if (R == 4 || R == 5) {
      vec2 v = voronoi(world * 0.085);
      float stone = smoothstep(0.03, 0.1, v.y);
      dirt *= mix(0.62, 1.0, stone) * (0.92 + hash(floor(world * 0.085 + 0.5)) * 0.12);
    } else {
      dirt = mix(dirt, DIRT_B * 0.8, smoothstep(0.80, 0.84, noise(world * 0.23)) * 0.5);
    }
    float edge = smoothstep(0.25, 0.43, pw) * (1.0 - path);
    vec3 col = mix(grass, mix(grass, dirt, 0.35) * 0.9, edge);
    col = mix(col, dirt, path);
    col *= 1.0 - (smoothstep(0.43, 0.47, pw) - smoothstep(0.47, 0.56, pw)) * 0.12;

    // liquids
    float ww = m.g + (fbm(world * 0.045) - 0.5) * 0.22;
    float liquid = smoothstep(0.48, 0.52, ww);
    float shore = smoothstep(0.32, 0.48, ww) * (1.0 - liquid);
    float depth = clamp(m.b + (fbm(world * 0.03) - 0.5) * 0.15, 0.0, 1.0);
    float lap = 0.5 + 0.035 * sin(uTime * 1.6 + world.x * 0.045 + world.y * 0.03);
    float foam = smoothstep(0.47, 0.5, ww) * (1.0 - smoothstep(lap, lap + 0.05, ww));
    vec3 lc = SHALLOW;
    float lem = 0.0;
    if (LQ == 0) {
      // water: sandy shore, depth, caustics, sky bands, glints, lapping foam
      col = mix(col, SHORE * (0.95 + noise(world * 0.3) * 0.08), shore * 0.85);
      col = mix(col, col * 0.78, smoothstep(0.42, 0.48, ww) * (1.0 - liquid));
      lc = mix(SHALLOW, DEEP, smoothstep(0.0, 1.0, depth));
      vec2 q = world * 0.045;
      float r1 = 1.0 - smoothstep(0.0, 0.07, abs(noise(q + vec2(uTime * 0.21, uTime * 0.13)) - 0.5));
      float r2 = 1.0 - smoothstep(0.0, 0.07, abs(noise(q * 1.6 - vec2(uTime * 0.17, -uTime * 0.19)) - 0.5));
      lc += (r1 * 0.5 + r2 * 0.5) * 0.09 * (1.0 - depth * 0.5);
      float band = noise(vec2(world.x * 0.008, world.y * 0.03) + vec2(uTime * 0.02, 0.0));
      lc = mix(lc, FOAM, smoothstep(0.6, 0.9, band) * 0.16);
      lc += smoothstep(0.94, 0.99, noise(world * 0.5 + vec2(uTime * 0.9, uTime * 0.6))) * 0.4;
      lc = mix(lc, FOAM, foam * 0.75);
    } else if (LQ == 1) {
      // bog: murky, slow swirls, an oily sheen, bubbles
      col = mix(col, SHORE * (0.9 + noise(world * 0.25) * 0.15), shore * 0.9);
      lc = mix(SHALLOW, DEEP, smoothstep(0.0, 0.8, depth));
      float sw = fbm(world * 0.018 + vec2(uTime * 0.015, -uTime * 0.01));
      lc *= 0.85 + sw * 0.3;
      float sheen = 1.0 - smoothstep(0.0, 0.08, abs(noise(world * 0.02 + vec2(uTime * 0.03, 0.0)) - 0.5));
      lc += vec3(0.08, 0.1, 0.06) * sheen;
      vec2 bc = floor(world * 0.07);
      float bt = fract(uTime * 0.25 + hash(bc) * 7.0);
      float bub = (1.0 - smoothstep(0.08, 0.18, length(fract(world * 0.07) - 0.5 - (hash2(bc) - 0.5) * 0.5))) * step(0.82, hash(bc + 3.0)) * smoothstep(0.0, 0.2, bt) * (1.0 - smoothstep(0.6, 0.8, bt));
      lc = mix(lc, FOAM, bub * 0.6);
      lc = mix(lc, FOAM, foam * 0.45);
      lem = 0.0;
    } else if (LQ == 2) {
      // lava: dark crust plates drifting on glowing rivers of fire
      col = mix(col, SHORE * (0.85 + noise(world * 0.3) * 0.2), shore);
      float rim = smoothstep(0.4, 0.5, ww) * (1.0 - liquid);
      col += GLOW * rim * 0.5;
      em = max(em, rim * 0.7);
      vec2 flow = vec2(uTime * 0.015, uTime * 0.006);
      vec2 v = voronoi(world * 0.03 + flow + vec2(fbm(world * 0.02), 0.0) * 0.6);
      float crack = 1.0 - smoothstep(0.0, 0.16 + depth * 0.1, v.y);
      float pulse = 0.85 + 0.15 * sin(uTime * 2.0 + world.x * 0.02);
      vec3 crust = DEEP * 0.22 * (0.8 + noise(world * 0.2) * 0.4);
      vec3 hot = mix(SHALLOW, FOAM, smoothstep(0.4, 1.0, crack)) * 1.7 * pulse;
      lc = mix(crust, hot, max(crack, (1.0 - depth) * 0.55));
      lc = mix(lc, FOAM * 1.8, foam * 0.5);
      lem = 1.0;
    } else if (LQ == 3) {
      // open sky far below: clouds drifting past with parallax, and rock faces under the cliff edges
      col = mix(col, col * 1.12 + vec3(0.05), smoothstep(0.38, 0.48, ww) * (1.0 - liquid));
      vec2 cp = uCam * 0.45 + (world - uCam) * 0.7;
      float c1 = fbm(cp * 0.004 + vec2(uTime * 0.012, uTime * 0.003));
      float c2 = fbm(cp * 0.009 - vec2(uTime * 0.02, 0.0) + 5.0);
      lc = mix(SHALLOW, DEEP, smoothstep(0.1, 1.0, depth) * 0.7);
      lc = mix(lc, FOAM, smoothstep(0.48, 0.75, c1) * 0.8);
      lc = mix(lc, FOAM * 0.92, smoothstep(0.55, 0.8, c2) * 0.4);
      float face = 0.0;
      for (int i = 1; i <= 7; i++) {
        if (mapLiquid(world - vec2(0.0, float(i) * 8.0)) < 0.45) { face = 1.0 - float(i - 1) / 7.0; break; }
      }
      vec3 rock = FLOOR * (0.55 + 0.45 * face) * (0.85 + 0.3 * noise(vec2(world.x * 0.06, world.y * 0.5)));
      rock *= 0.9 + 0.2 * step(0.5, fract(world.y * 0.08 + noise(world * 0.05)));
      lc = mix(lc, rock, step(0.001, face));
      lem = 0.65 * (1.0 - step(0.001, face));
    } else {
      // Malgrin's goo: slow purple swirls with glowing veins and bubbles
      col = mix(col, SHORE * (0.85 + noise(world * 0.3) * 0.2), shore * 0.9);
      float sw = fbm(world * 0.02 + vec2(uTime * 0.05, -uTime * 0.03));
      lc = mix(DEEP, SHALLOW, smoothstep(0.35, 0.75, sw));
      float vein = 1.0 - smoothstep(0.0, 0.06, abs(noise(world * 0.03 + vec2(uTime * 0.04, uTime * 0.02)) - 0.5));
      lc += GLOW * vein * 0.7;
      vec2 bc = floor(world * 0.06);
      float bt = fract(uTime * 0.3 + hash(bc) * 5.0);
      float bub = (1.0 - smoothstep(0.1, 0.2, length(fract(world * 0.06) - 0.5))) * step(0.75, hash(bc + 1.0)) * (1.0 - smoothstep(0.5, 0.9, bt));
      lc = mix(lc, FOAM, bub * 0.7);
      lc = mix(lc, FOAM, foam * 0.5);
      lem = 0.3 + vein * 0.5 + bub * 0.4;
    }
    col = mix(col, lc, liquid);
    em = mix(em, lem, liquid);
    o = vec4(col, clamp(em, 0.0, 1.0));
  }`;

  const LIGHT_VS = `#version 300 es
  layout(location = 0) in vec2 aCorner; layout(location = 1) in vec4 aLight; layout(location = 2) in vec4 aColor;
  uniform vec2 uCam; uniform float uZoom; uniform vec2 uView;
  out vec2 vLocal; out vec3 vColor; out float vInt;
  void main() {
    vec2 p = aLight.xy + aCorner * aLight.z;
    vec2 v = (p - uCam) * uZoom;
    gl_Position = vec4(v.x * 2.0 / uView.x, -v.y * 2.0 / uView.y, 0.0, 1.0);
    vLocal = aCorner; vColor = aColor.rgb; vInt = aLight.w;
  }`;
  const LIGHT_FS = `#version 300 es
  precision mediump float;
  in vec2 vLocal; in vec3 vColor; in float vInt; out vec4 o;
  void main() { float f = clamp(1.0 - dot(vLocal, vLocal), 0.0, 1.0); o = vec4(vColor * f * f * vInt * 0.5, 1.0); }`;

  const COMPOSITE_FS = `#version 300 es
  precision highp float;
  uniform sampler2D uScene, uLight; uniform vec2 uCam; uniform float uZoom; uniform vec2 uView; uniform float uTime; uniform vec3 uAmb;
  uniform vec4 uFog;
  in vec2 vUV; out vec4 o;
  ${NOISE}
  void main() {
    vec4 sc = texture(uScene, vUV);
    vec3 s = sc.rgb;
    vec3 raw = texture(uLight, vUV).rgb * 2.0;
    vec3 extra = max(raw - uAmb, 0.0);
    // lamps count for less where the land is already bright, so nothing washes out to white
    float room = clamp(1.5 - dot(uAmb, vec3(0.3333)), 0.35, 1.0);
    vec3 l = uAmb + (1.0 - exp(-extra * 1.4)) * 0.6 * room;
    vec2 world = worldAt(gl_FragCoord.xy);
    float c = fbm(world * 0.0016 + vec2(uTime * 0.012, uTime * 0.004));
    float cloud = smoothstep(0.5, 0.72, c);
    // lava, open sky and glowing goo shine by themselves (the ground pass marks them in alpha)
    vec3 col = mix(s * l * (1.0 - cloud * 0.2), s, clamp(sc.a, 0.0, 1.0));
    // drifting fog banks
    float f = fbm(world * 0.0032 + vec2(uTime * 0.014, uTime * 0.005));
    float fk = uFog.a * smoothstep(0.3, 0.78, f);
    col = mix(col, uFog.rgb * (uAmb * 0.5 + 0.5), fk);
    o = vec4(col, 1.0);
  }`;

  const BRIGHT_FS = `#version 300 es
  precision mediump float;
  uniform sampler2D uSrc; in vec2 vUV; out vec4 o;
  void main() { vec3 c = texture(uSrc, vUV).rgb; float m = max(c.r, max(c.g, c.b)); o = vec4(c * smoothstep(0.9, 1.5, m), 1.0); }`;
  const DOWN_FS = `#version 300 es
  precision mediump float;
  uniform sampler2D uSrc; uniform vec2 uTexel; in vec2 vUV; out vec4 o;
  void main() {
    vec2 h = uTexel * 0.5;
    vec4 s = texture(uSrc, vUV) * 4.0;
    s += texture(uSrc, vUV - h); s += texture(uSrc, vUV + h);
    s += texture(uSrc, vUV + vec2(h.x, -h.y)); s += texture(uSrc, vUV - vec2(h.x, -h.y));
    o = s / 8.0;
  }`;
  const UP_FS = `#version 300 es
  precision mediump float;
  uniform sampler2D uSrc; uniform vec2 uTexel; in vec2 vUV; out vec4 o;
  void main() {
    vec2 h = uTexel * 0.5;
    vec4 s = texture(uSrc, vUV + vec2(-h.x * 2.0, 0.0));
    s += texture(uSrc, vUV + vec2(-h.x, h.y)) * 2.0;
    s += texture(uSrc, vUV + vec2(0.0, h.y * 2.0));
    s += texture(uSrc, vUV + vec2(h.x, h.y)) * 2.0;
    s += texture(uSrc, vUV + vec2(h.x * 2.0, 0.0));
    s += texture(uSrc, vUV + vec2(h.x, -h.y)) * 2.0;
    s += texture(uSrc, vUV + vec2(0.0, -h.y * 2.0));
    s += texture(uSrc, vUV + vec2(-h.x, -h.y)) * 2.0;
    o = s / 12.0;
  }`;
  const FINAL_FS = `#version 300 es
  precision highp float;
  uniform sampler2D uComp, uBloom; uniform float uBloomK, uVignette, uSat, uFade, uTime; uniform vec3 uGrade; uniform vec2 uView;
  in vec2 vUV; out vec4 o;
  float hash(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
  void main() {
    vec3 c = texture(uComp, vUV).rgb + texture(uBloom, vUV).rgb * uBloomK;
    vec3 over = max(c - 0.82, 0.0);
    c = min(c, 0.82) + over / (1.0 + over * 2.6);
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(l), c, uSat) * uGrade;
    vec2 d = vUV - 0.5; d.x *= uView.x / uView.y;
    c *= mix(1.0 - uVignette, 1.0, smoothstep(0.95, 0.3, length(d)));
    c += (hash(gl_FragCoord.xy + fract(uTime) * 61.0) - 0.5) * (1.5 / 255.0);
    o = vec4(c * (1.0 - uFade), 1.0);
  }`;

  const P = {
    sprite: program(SPRITE_VS, SPRITE_FS),
    ground: program(FULL_VS, GROUND_FS),
    light: program(LIGHT_VS, LIGHT_FS),
    composite: program(FULL_VS, COMPOSITE_FS),
    bright: program(FULL_VS, BRIGHT_FS),
    down: program(FULL_VS, DOWN_FS),
    up: program(FULL_VS, UP_FS),
    final: program(FULL_VS, FINAL_FS),
  };

  // ---------------------------------------------------------------- buffers
  const emptyVao = gl.createVertexArray();

  const spriteVao = gl.createVertexArray();
  gl.bindVertexArray(spriteVao);
  const spriteVbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, spriteVbo);
  gl.bufferData(gl.ARRAY_BUFFER, MAX_QUADS * 4 * 24, gl.DYNAMIC_DRAW);
  gl.enableVertexAttribArray(0); gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 24, 0);
  gl.enableVertexAttribArray(1); gl.vertexAttribPointer(1, 2, gl.FLOAT, false, 24, 8);
  gl.enableVertexAttribArray(2); gl.vertexAttribPointer(2, 4, gl.UNSIGNED_BYTE, true, 24, 16);
  gl.enableVertexAttribArray(3); gl.vertexAttribPointer(3, 4, gl.UNSIGNED_BYTE, true, 24, 20);
  const idx = new Uint32Array(MAX_QUADS * 6);
  for (let q = 0; q < MAX_QUADS; q++) {
    const v = q * 4, i = q * 6;
    idx[i] = v; idx[i + 1] = v + 1; idx[i + 2] = v + 2; idx[i + 3] = v; idx[i + 4] = v + 2; idx[i + 5] = v + 3;
  }
  const ibo = gl.createBuffer();
  gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, ibo);
  gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, idx, gl.STATIC_DRAW);

  const lightVao = gl.createVertexArray();
  gl.bindVertexArray(lightVao);
  const cornerVbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, cornerVbo);
  gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 1, -1, -1, 1, 1, 1]), gl.STATIC_DRAW);
  gl.enableVertexAttribArray(0); gl.vertexAttribPointer(0, 2, gl.FLOAT, false, 8, 0);
  const lightVbo = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, lightVbo);
  gl.bufferData(gl.ARRAY_BUFFER, MAX_LIGHTS * 32, gl.DYNAMIC_DRAW);
  gl.enableVertexAttribArray(1); gl.vertexAttribPointer(1, 4, gl.FLOAT, false, 32, 0); gl.vertexAttribDivisor(1, 1);
  gl.enableVertexAttribArray(2); gl.vertexAttribPointer(2, 4, gl.FLOAT, false, 32, 16); gl.vertexAttribDivisor(2, 1);
  gl.bindVertexArray(null);

  // ---------------------------------------------------------------- textures
  const atlasTex = gl.createTexture();
  let atlasVersion = -1;
  function uploadAtlas() {
    gl.bindTexture(gl.TEXTURE_2D, atlasTex);
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 4);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, atlasSize, atlasSize, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(memory.buffer, ptr.atlas, atlasSize * atlasSize * 4));
    gl.generateMipmap(gl.TEXTURE_2D);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
    if (aniso) gl.texParameterf(gl.TEXTURE_2D, aniso.TEXTURE_MAX_ANISOTROPY_EXT, 4);
  }
  const mapTex = gl.createTexture();
  let mapVersion = -1;
  function uploadMap(w, h) {
    gl.bindTexture(gl.TEXTURE_2D, mapTex);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA8, w, h, 0, gl.RGBA, gl.UNSIGNED_BYTE, new Uint8Array(memory.buffer, X.game_ground(), w * h * 4));
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
    gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  }

  // ---------------------------------------------------------------- render targets
  let W = 0, H = 0, dpr = 1, cssW = 0, cssH = 0;
  let scene, lightmap, comp, bloom = [];
  function resize() {
    cssW = canvas.clientWidth || window.innerWidth;
    cssH = canvas.clientHeight || window.innerHeight;
    const maxPixels = 3.8e6;
    dpr = Math.min(window.devicePixelRatio || 1, 2, Math.sqrt(maxPixels / (cssW * cssH)));
    const w = Math.max(1, Math.round(cssW * dpr)), h = Math.max(1, Math.round(cssH * dpr));
    if (w === W && h === H) return;
    W = w; H = h;
    canvas.width = W; canvas.height = H;
    [scene, lightmap, comp, ...bloom].forEach(free);
    const fmt = hdr ? 'hdr' : 'ldr';
    scene = target(W, H, fmt);
    lightmap = target(Math.max(1, W >> 1), Math.max(1, H >> 1), fmt);
    comp = target(W, H, fmt);
    bloom = [];
    let bw = W >> 1, bh = H >> 1;
    for (let i = 0; i < 5; i++) { bloom.push(target(Math.max(1, bw), Math.max(1, bh), fmt)); bw >>= 1; bh >>= 1; }
  }
  window.addEventListener('resize', resize);
  resize();

  // ---------------------------------------------------------------- input
  const KEYS = {
    KeyW: BTN.UP, ArrowUp: BTN.UP, KeyS: BTN.DOWN, ArrowDown: BTN.DOWN, KeyA: BTN.LEFT, ArrowLeft: BTN.LEFT, KeyD: BTN.RIGHT, ArrowRight: BTN.RIGHT,
    Space: BTN.DASH, ShiftLeft: BTN.DASH, ShiftRight: BTN.DASH, KeyF: BTN.FIRE, KeyK: BTN.FIRE, KeyE: BTN.INTERACT, Enter: BTN.INTERACT, Escape: BTN.PAUSE, KeyP: BTN.PAUSE,
  };
  let held = 0, pressed = 0;
  const typing = (e) => e.target && /^(INPUT|TEXTAREA|SELECT)$/.test(e.target.tagName);
  window.addEventListener('keydown', (e) => {
    if (typing(e)) return;
    const b = KEYS[e.code];
    if (!b) return;
    if (e.code.startsWith('Arrow') || e.code === 'Space') e.preventDefault();
    if (!e.repeat) pressed |= b;
    held |= b;
  });
  window.addEventListener('keyup', (e) => { const b = KEYS[e.code]; if (b) held &= ~b; });
  window.addEventListener('blur', () => { held = 0; });
  let padPrev = 0;
  function pollPad() {
    const pads = navigator.getGamepads ? navigator.getGamepads() : [];
    let mx = 0, my = 0, bits = 0;
    for (const p of pads) {
      if (!p) continue;
      mx = p.axes[0] || 0; my = p.axes[1] || 0;
      const b = (i) => p.buttons[i] && p.buttons[i].pressed;
      if (b(0) || b(5)) bits |= BTN.DASH;
      if (b(2) || b(7)) bits |= BTN.FIRE;
      if (b(1)) bits |= BTN.INTERACT;
      if (b(9)) bits |= BTN.PAUSE;
      if (b(12)) bits |= BTN.UP; if (b(13)) bits |= BTN.DOWN; if (b(14)) bits |= BTN.LEFT; if (b(15)) bits |= BTN.RIGHT;
      break;
    }
    const newly = bits & ~padPrev;
    padPrev = bits;
    return { mx, my, bits, newly };
  }
  const stick = { x: 0, y: 0 };
  window.SkyInput = { press(b) { pressed |= b; }, hold(b, on) { held = on ? held | b : held & ~b; }, stick(x, y) { stick.x = x; stick.y = y; }, BTN };

  // ---------------------------------------------------------------- frame
  function bindFull(t) { gl.bindFramebuffer(gl.FRAMEBUFFER, t ? t.fb : null); gl.viewport(0, 0, t ? t.w : W, t ? t.h : H); }
  function drawFull(prog) { gl.useProgram(prog.p); gl.bindVertexArray(emptyVao); gl.drawArrays(gl.TRIANGLES, 0, 3); }
  function tex(unit, t, loc) { gl.activeTexture(gl.TEXTURE0 + unit); gl.bindTexture(gl.TEXTURE_2D, t); gl.uniform1i(loc, unit); }

  let last = performance.now();
  const stats = { frames: 0, ms: 0, worst: 0, bakeMs };
  window.SkyStats = stats;
  window.SkyGame = X;

  function frame(now) {
    const dt = Math.min(0.1, (now - last) / 1000);
    last = now;
    resize();

    const pad = pollPad();
    const F = f32();
    const inp = ptr.input >> 2;
    F[inp + 0] = Math.abs(pad.mx) > 0.15 ? pad.mx : stick.x;
    F[inp + 1] = Math.abs(pad.my) > 0.15 ? pad.my : stick.y;
    const U = u32();
    U[inp + 2] = held | pad.bits;
    U[inp + 3] = pressed | pad.newly;
    F[inp + 4] = W; F[inp + 5] = H; F[inp + 6] = dpr; F[inp + 7] = cssH;
    pressed = 0;

    const tStart = performance.now();
    X.game_frame(dt);
    pumpAudio();
    const tGame = performance.now() - tStart;

    const f = f32();
    const info = ptr.info >> 2;
    const I = (k) => f[info + k];
    if (I(FI.ATLAS_VERSION) !== atlasVersion) { atlasVersion = I(FI.ATLAS_VERSION); uploadAtlas(); }
    if (I(FI.MAP_VERSION) !== mapVersion) { mapVersion = I(FI.MAP_VERSION); uploadMap(I(FI.MAP_W), I(FI.MAP_H)); }
    const camX = I(FI.CAM_X) + I(FI.SHAKE_X), camY = I(FI.CAM_Y) + I(FI.SHAKE_Y), zoom = I(FI.ZOOM), time = I(FI.TIME);
    const allQuads = I(FI.ALL_QUADS) | 0, litQuads = I(FI.LIT_QUADS) | 0, lights = I(FI.LIGHTS) | 0;

    // 1. ground
    bindFull(scene);
    gl.disable(gl.BLEND);
    const G = P.ground;
    gl.useProgram(G.p);
    gl.uniform2f(G.u.uCam, camX, camY); gl.uniform1f(G.u.uZoom, zoom); gl.uniform2f(G.u.uView, W, H); gl.uniform1f(G.u.uTime, time);
    gl.uniform2f(G.u.uMapSize, I(FI.MAP_W), I(FI.MAP_H)); gl.uniform1f(G.u.uTile, I(FI.TILE));
    const pal = new Float32Array(memory.buffer, ptr.palette, 40);
    gl.uniform3fv(G.u['uPal[0]'], pal.subarray(0, 33));
    gl.uniform1f(G.u.uLiquid, pal[37]); gl.uniform1f(G.u.uRegion, pal[38]);
    tex(0, mapTex, G.u.uMap);
    drawFull(G);

    // 2. lit sprites, one batch
    gl.bindBuffer(gl.ARRAY_BUFFER, spriteVbo);
    gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Uint8Array(memory.buffer, ptr.verts, allQuads * 96));
    const SP = P.sprite;
    gl.useProgram(SP.p);
    gl.uniform2f(SP.u.uCam, camX, camY); gl.uniform1f(SP.u.uZoom, zoom); gl.uniform2f(SP.u.uView, W, H);
    gl.uniform1f(SP.u.uTime, time); gl.uniform1f(SP.u.uWind, I(FI.WIND));
    tex(0, atlasTex, SP.u.uTex);
    gl.enable(gl.BLEND);
    // colour blends normally; alpha keeps the ground's "shines by itself" mark only where nothing covers it
    gl.blendFuncSeparate(gl.ONE, gl.ONE_MINUS_SRC_ALPHA, gl.ZERO, gl.ONE_MINUS_SRC_ALPHA);
    gl.bindVertexArray(spriteVao);
    if (litQuads > 0) gl.drawElements(gl.TRIANGLES, litQuads * 6, gl.UNSIGNED_INT, 0);

    // 3. light map: ambient plus every light, additively
    bindFull(lightmap);
    gl.clearColor(I(FI.AMB_R) * 0.5, I(FI.AMB_G) * 0.5, I(FI.AMB_B) * 0.5, 1);
    gl.clear(gl.COLOR_BUFFER_BIT);
    if (lights > 0) {
      gl.blendFunc(gl.ONE, gl.ONE);
      gl.bindBuffer(gl.ARRAY_BUFFER, lightVbo);
      gl.bufferSubData(gl.ARRAY_BUFFER, 0, new Float32Array(memory.buffer, ptr.lights, lights * 8));
      const L = P.light;
      gl.useProgram(L.p);
      gl.uniform2f(L.u.uCam, camX, camY); gl.uniform1f(L.u.uZoom, zoom); gl.uniform2f(L.u.uView, W, H);
      gl.bindVertexArray(lightVao);
      gl.drawArraysInstanced(gl.TRIANGLE_STRIP, 0, 4, lights);
    }

    // 4. composite scene x light, then glow on top
    bindFull(comp);
    gl.disable(gl.BLEND);
    const C = P.composite;
    gl.useProgram(C.p);
    tex(0, scene.tex, C.u.uScene); tex(1, lightmap.tex, C.u.uLight);
    gl.uniform2f(C.u.uCam, camX, camY); gl.uniform1f(C.u.uZoom, zoom); gl.uniform2f(C.u.uView, W, H); gl.uniform1f(C.u.uTime, time);
    gl.uniform3f(C.u.uAmb, I(FI.AMB_R), I(FI.AMB_G), I(FI.AMB_B));
    gl.uniform4f(C.u.uFog, pal[33], pal[34], pal[35], pal[36]);
    drawFull(C);
    if (allQuads > litQuads) {
      gl.enable(gl.BLEND);
      gl.blendFunc(gl.ONE, gl.ONE_MINUS_SRC_ALPHA);
      gl.useProgram(SP.p);
      tex(0, atlasTex, SP.u.uTex);
      gl.bindVertexArray(spriteVao);
      gl.drawElements(gl.TRIANGLES, (allQuads - litQuads) * 6, gl.UNSIGNED_INT, litQuads * 6 * 4);
    }

    // 5. bloom: bright pass, then down and back up the chain
    gl.disable(gl.BLEND);
    bindFull(bloom[0]);
    gl.useProgram(P.bright.p);
    tex(0, comp.tex, P.bright.u.uSrc);
    drawFull(P.bright);
    for (let i = 1; i < bloom.length; i++) {
      bindFull(bloom[i]);
      gl.useProgram(P.down.p);
      tex(0, bloom[i - 1].tex, P.down.u.uSrc);
      gl.uniform2f(P.down.u.uTexel, 1 / bloom[i - 1].w, 1 / bloom[i - 1].h);
      drawFull(P.down);
    }
    gl.enable(gl.BLEND);
    gl.blendFunc(gl.ONE, gl.ONE);
    for (let i = bloom.length - 1; i > 0; i--) {
      bindFull(bloom[i - 1]);
      gl.useProgram(P.up.p);
      tex(0, bloom[i].tex, P.up.u.uSrc);
      gl.uniform2f(P.up.u.uTexel, 1 / bloom[i].w, 1 / bloom[i].h);
      drawFull(P.up);
    }

    // 6. final grade to the screen
    gl.disable(gl.BLEND);
    bindFull(null);
    const FN = P.final;
    gl.useProgram(FN.p);
    tex(0, comp.tex, FN.u.uComp); tex(1, bloom[0].tex, FN.u.uBloom);
    gl.uniform1f(FN.u.uBloomK, I(FI.BLOOM)); gl.uniform1f(FN.u.uVignette, I(FI.VIGNETTE)); gl.uniform1f(FN.u.uSat, I(FI.SATURATION));
    gl.uniform1f(FN.u.uFade, I(FI.FADE)); gl.uniform1f(FN.u.uTime, time);
    gl.uniform3f(FN.u.uGrade, I(FI.GRADE_R), I(FI.GRADE_G), I(FI.GRADE_B)); gl.uniform2f(FN.u.uView, W, H);
    drawFull(FN);

    const dbg = ptr.debug >> 2;
    const sp = ptr.speakers >> 2;
    const toScreen = (x, y) => ({ x: (x - camX) * zoom / dpr + cssW / 2, y: (y - camY) * zoom / dpr + cssH / 2 });
    ui.frame({
      flint: toScreen(f[sp + 0], f[sp + 1]),
      hero: toScreen(f[dbg + 0], f[dbg + 1]),
      speaker: (i) => toScreen(f[sp + i * 2], f[sp + i * 2 + 1]),
    });

    stats.frames++;
    stats.ms += tGame;
    stats.worst = Math.max(stats.worst, tGame);
    stats.quads = allQuads;
    stats.lights = lights;
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);
})().catch((e) => {
  console.error(e);
  if (window.SkyUI) window.SkyUI.fail('Something went wrong while starting the game: ' + e.message);
});
