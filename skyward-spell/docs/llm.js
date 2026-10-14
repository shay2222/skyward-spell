// llm.js - the optional live language model for character dialogue.
//
// The model runs entirely in the player's browser with WebLLM (WebGPU): free, no account, no
// server, and nothing the player types leaves their machine. It is off until the player turns it
// on, because the first load downloads the model (hundreds of MB, cached by the browser after).
//
// The C++ game decides everything that matters: it builds the prompt from facts that are true
// right now, and its guard checks every line the model writes. If a line fails a check, or the
// model is slow or missing, the offline generator answers instead, so the game never depends on it.
'use strict';

window.SkyLLM = (function () {
  const WEBLLM = 'https://cdn.jsdelivr.net/npm/@mlc-ai/web-llm@0.2.85/+esm';
  const MODELS = {
    better: { id: 'Llama-3.2-1B-Instruct-q4f16_1-MLC', label: 'Llama 3.2 1B' },
    small: { id: 'Qwen2.5-0.5B-Instruct-q4f16_1-MLC', label: 'Qwen 2.5 0.5B' },
  };
  let engine = null;
  let state = 'off';         // off | loading | ready | error | unsupported
  let detail = '';
  let model = null;
  const listeners = [];

  function set(s, d) {
    state = s;
    detail = d || '';
    listeners.forEach((f) => { try { f(state, detail); } catch (e) { /* a listener's problem, not ours */ } });
  }

  async function enable(which) {
    if (state === 'loading' || state === 'ready') return state === 'ready';
    if (!('gpu' in navigator)) {
      set('unsupported', 'This browser has no WebGPU, so the live model can\'t run here. Try a recent Chrome or Edge on a computer.');
      return false;
    }
    const m = MODELS[which] || MODELS.better;
    set('loading', 'Loading the model library…');
    try {
      const webllm = await import(WEBLLM);
      engine = await webllm.CreateMLCEngine(m.id, {
        initProgressCallback: (p) => set('loading', p.text || Math.round((p.progress || 0) * 100) + '%'),
      });
      model = m;
      set('ready', m.label);
      return true;
    } catch (e) {
      engine = null;
      set('error', 'The model couldn\'t load: ' + (e && e.message ? e.message : e));
      return false;
    }
  }

  async function ask(prompt) {
    if (!engine) throw new Error('no model loaded');
    const req = {
      messages: [
        { role: 'system', content: prompt },
        { role: 'user', content: 'Reply now, in character, as JSON only.' },
      ],
      temperature: 0.8,
      max_tokens: 120,
    };
    let r;
    try {
      r = await engine.chat.completions.create(Object.assign({ response_format: { type: 'json_object' } }, req));
    } catch (e) {
      r = await engine.chat.completions.create(req);  // some models don't do JSON mode: ask plainly
    }
    return (r && r.choices && r.choices[0] && r.choices[0].message && r.choices[0].message.content) || '';
  }

  return {
    MODELS,
    enable,
    ask,
    on(f) { listeners.push(f); f(state, detail); },
    get state() { return state; },
    get detail() { return detail; },
    get model() { return model; },
  };
})();
