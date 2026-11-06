const $ = id => document.getElementById(id);
const API = (window.API_BASE || '').replace(/\/$/, '');
const UNREACHABLE = 'Cannot reach the server. If it is hosted on a free plan it may be waking up, so try again in a minute.';
const inputBody = $('input').tBodies[0];
let compareOn = false;
let lastRun = null;
let colors = {};

function addRow(pid, arrival, burst, priority) {
  const tr = document.createElement('tr');
  [pid, arrival, burst, priority].forEach((v, i) => {
    const td = document.createElement('td');
    const inp = document.createElement('input');
    inp.type = 'number'; inp.step = '1'; inp.value = v;
    inp.setAttribute('aria-label', ['PID', 'Arrival', 'Burst', 'Priority'][i]);
    td.appendChild(inp); tr.appendChild(td);
  });
  const td = document.createElement('td');
  const btn = document.createElement('button');
  btn.type = 'button'; btn.className = 'x'; btn.textContent = '✕';
  btn.setAttribute('aria-label', 'Remove process');
  btn.onclick = () => tr.remove();
  td.appendChild(btn); tr.appendChild(td);
  inputBody.appendChild(tr);
}

function nextPid() {
  const ids = [...inputBody.rows].map(r => parseInt(r.cells[0].firstChild.value, 10)).filter(Number.isFinite);
  return ids.length ? Math.max(...ids) + 1 : 1;
}

function readProcesses() {
  const procs = [], seen = new Set();
  const rows = [...inputBody.rows];
  if (!rows.length) return { error: 'Add at least one process.' };
  for (let i = 0; i < rows.length; i++) {
    const v = [...rows[i].querySelectorAll('input')].map(x => x.value.trim());
    const n = v.map(s => (/^-?\d+$/.test(s) ? Number(s) : NaN));
    if (v[3] === '') n[3] = 0;
    const who = 'Row ' + (i + 1) + ': ';
    if (n.some(Number.isNaN)) return { error: who + 'every field must be a whole number.' };
    const [pid, arrival, burst, priority] = n;
    if (pid < 0 || arrival < 0) return { error: who + 'PID and arrival must not be negative.' };
    if (burst < 1) return { error: who + 'burst time must be at least 1.' };
    if (seen.has(pid)) return { error: who + 'PID ' + pid + ' is already used.' };
    seen.add(pid);
    procs.push({ pid, arrival, burst, priority });
  }
  return { procs };
}

async function request(body) {
  const res = await fetch(API + '/run', {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
  });
  const data = await res.json();
  if (!res.ok) throw new Error(data.error || 'The server rejected the request.');
  return data;
}

function build(algo, procs, quantum, preemptive) {
  return algo === 'rr'
    ? { algorithm: 'rr', quantum, processes: procs }
    : { algorithm: 'priority', preemptive, processes: procs };
}

async function go(compare) {
  setError('');
  const { procs, error } = readProcesses();
  if (error) return setError(error);

  const algo = $('algo').value;
  const quantum = Number($('quantum').value);
  const preemptive = $('pre').checked;
  if ((algo === 'rr' || compare) && !(Number.isInteger(quantum) && quantum >= 1))
    return setError('Time quantum must be a whole number of at least 1.');

  compareOn = compare;
  syncControls();
  try {
    const main = await request(build(algo, procs, quantum, preemptive));
    render(main);
    lastRun = { algo, quantum, preemptive };
    $('liveP').hidden = algo === 'rr';
    $('liveMsg').textContent = '';
    if (compare) {
      const other = await request(build(algo === 'rr' ? 'priority' : 'rr', procs, quantum, preemptive));
      openCompare(algo === 'rr' ? [main, other] : [other, main]);
    }
  } catch (e) {
    $('out').hidden = true;
    setError(e instanceof TypeError ? UNREACHABLE : e.message);
  }
}

function el(tag, cls, text) {
  const e = document.createElement(tag);
  if (cls) e.className = cls;
  if (text !== undefined) e.textContent = text;
  return e;
}

function label(r) {
  return r.algorithm === 'rr'
    ? 'Round Robin (quantum ' + r.quantum + ')'
    : 'Priority (' + (r.preemptive ? 'preemptive' : 'non-preemptive') + ')';
}

function render(r, keepTime) {
  $('out').hidden = false;
  colors = {};
  r.processes.forEach((p, i) => { colors[p.pid] = 'hsl(' + ((i * 67) % 360) + ' 62% 40%)'; });

  const g = $('gantt');
  g.textContent = '';
  r.gantt.forEach((e, i) => {
    const b = el('div', 'block' + (e.idle ? ' idle' : ''), e.idle ? 'idle' : 'P' + e.pid);
    b.style.flex = (e.end - e.start) + ' 1 0';
    if (!e.idle) b.style.background = colors[e.pid];
    b.title = (e.idle ? 'CPU idle' : 'P' + e.pid) + ', time ' + e.start + ' to ' + e.end;
    b.appendChild(el('span', 't0', e.start));
    if (i === r.gantt.length - 1) b.appendChild(el('span', 't1', e.end));
    g.appendChild(b);
  });

  $('summary').textContent = label(r) + ': finished at time ' + r.totalTime +
    ', CPU idle for ' + r.idleTime + ' time units.';

  const body = $('results').tBodies[0];
  body.textContent = '';
  r.processes.forEach(p => {
    const tr = el('tr');
    const name = el('td', '', 'P' + p.pid);
    name.style.cssText = 'font-weight:700;border-left:6px solid ' + colors[p.pid];
    tr.appendChild(name);
    [p.arrival, p.burst, p.priority, p.completion, p.turnaround, p.waiting, p.response]
      .forEach(v => tr.appendChild(el('td', '', v)));
    body.appendChild(tr);
  });

  const foot = $('results').tFoot;
  foot.textContent = '';
  const tr = el('tr');
  const first = el('td', '', 'Average');
  first.colSpan = 5;
  tr.append(first, el('td', '', r.averageTurnaround.toFixed(2)), el('td', '', r.averageWaiting.toFixed(2)), el('td', '', r.averageResponse.toFixed(2)));
  foot.appendChild(tr);
  showMetrics(r);
  startPlayer(r, keepTime);
}


function setError(msg) { $('error').textContent = msg; }

function syncControls() {
  const a = $('algo').value;
  $('quantumBox').hidden = !(a === 'rr' || compareOn);
  $('preBox').hidden = !(a === 'priority' || compareOn);
}

$('add').onclick = () => addRow(nextPid(), 0, 3, 1);
$('run').onclick = () => go(false);
$('compare').onclick = () => go(true);
$('algo').onchange = () => { compareOn = false; syncControls(); };

addRow(1, 0, 5, 1);
addRow(2, 1, 3, 2);
addRow(3, 2, 1, 3);
syncControls();

let sim = null, byPid = {}, barRows = {}, T = 0, playing = false, lastFrame = 0;

function chip(pid) {
  const c = el('span', 'chip', 'P' + pid);
  c.style.background = colors[pid];
  return c;
}

function startPlayer(r, keepTime) {
  pause();
  sim = r;
  byPid = {};
  r.processes.forEach(p => { byPid[p.pid] = p; });
  const ph = el('div'); ph.id = 'playhead';
  $('gantt').appendChild(ph);

  const bars = $('bars');
  bars.textContent = '';
  barRows = {};
  r.processes.forEach(p => {
    const row = el('div', 'bar'), track = el('div', 'track'), fill = el('div', 'fill'), info = el('span');
    const name = el('b', '', 'P' + p.pid);
    name.style.borderLeft = '6px solid ' + colors[p.pid];
    fill.style.background = colors[p.pid];
    track.appendChild(fill);
    row.append(name, track, info);
    bars.appendChild(row);
    barRows[p.pid] = { fill, info };
  });
  setTime(keepTime === undefined ? r.totalTime : keepTime);
}

function setTime(t) {
  T = Math.max(0, Math.min(sim.totalTime, t));
  const blocks = [...$('gantt').querySelectorAll('.block')];
  let cur = null, at = blocks[blocks.length - 1];
  blocks.forEach((b, i) => {
    const e = sim.gantt[i];
    const now = e.start <= T && T < e.end;
    b.classList.toggle('now', now);
    b.classList.toggle('future', e.start > T);
    if (now) { cur = e; at = b; }
  });
  const e = cur || sim.gantt[sim.gantt.length - 1];
  const frac = Math.min(1, Math.max(0, (T - e.start) / (e.end - e.start)));
  $('playhead').style.left = (at.offsetLeft + frac * at.offsetWidth) + 'px';

  const ran = {};
  sim.gantt.forEach(g => {
    if (!g.idle) ran[g.pid] = (ran[g.pid] || 0) + Math.max(0, Math.min(g.end, T) - g.start);
  });

  const running = cur && !cur.idle ? cur.pid : null;
  let q = [];
  if (running !== null) {
    const inQ = new Set(cur.queue);
    q = cur.queue.slice();
    sim.processes
      .filter(p => p.arrival > cur.start && p.arrival <= T && !inQ.has(p.pid) && p.pid !== running)
      .sort((a, b) => a.arrival - b.arrival || a.pid - b.pid)
      .forEach(p => q.push(p.pid));
    if (sim.algorithm === 'priority')
      q.sort((x, y) => byPid[y].priority - byPid[x].priority || byPid[x].arrival - byPid[y].arrival || x - y);
  }

  $('clock').textContent = 'Clock ' + T.toFixed(1);
  updateLiveNote();
  const run = $('running'), queue = $('queue');
  run.textContent = ''; queue.textContent = '';
  if (running !== null) run.appendChild(chip(running));
  else run.textContent = T >= sim.totalTime ? 'finished' : 'idle';
  if (q.length) q.forEach(id => queue.appendChild(chip(id)));
  else queue.textContent = 'empty';

  sim.processes.forEach((p, i) => {
    const left = Math.max(0, p.burst - (ran[p.pid] || 0));
    const bar = barRows[p.pid];
    bar.fill.style.width = (left / p.burst * 100) + '%';
    bar.info.textContent = p.completion <= T ? 'done at ' + p.completion
      : p.pid === running ? 'running, ' + Math.ceil(left) + ' left'
      : p.arrival <= T ? 'waiting, ' + Math.ceil(left) + ' left'
      : 'arrives at ' + p.arrival;
    $('results').tBodies[0].rows[i].classList.toggle('fin', p.completion <= T);
  });
}

function tick(now) {
  if (!playing) return;
  const dt = (now - lastFrame) / 1000;
  lastFrame = now;
  setTime(T + dt * Number($('speed').value));
  if (T >= sim.totalTime) return pause();
  requestAnimationFrame(tick);
}
function play() {
  if (!sim) return;
  if (T >= sim.totalTime) setTime(0);
  playing = true;
  $('play').textContent = 'Pause';
  lastFrame = performance.now();
  requestAnimationFrame(tick);
}
function pause() { playing = false; $('play').textContent = 'Play'; }

$('play').onclick = () => (playing ? pause() : play());
$('restart').onclick = () => { pause(); setTime(0); };
$('fwd').onclick = () => { pause(); setTime(Math.floor(T + 1e-9) + 1); };
$('back').onclick = () => { pause(); setTime(Math.ceil(T - 1e-9) - 1); };

function liveArrival() {
  const d = Number($('liveDelay').value);
  return Math.max(0, Math.ceil(T - 1e-9)) + (Number.isInteger(d) && d > 0 ? d : 0);
}

function updateLiveNote() {
  $('liveNote').textContent = 'It will arrive at clock ' + liveArrival() + '. Everything before that stays exactly as it is.';
}

async function addLive() {
  setError('');
  $('liveMsg').textContent = '';
  if (!sim || !lastRun) return setError('Run a simulation first.');
  const num = (id, fallback) => ($(id).value.trim() === '' ? fallback : Number($(id).value));
  const burst = num('liveBurst', NaN), priority = num('livePriority', 0), delay = num('liveDelay', 0);
  if (!Number.isInteger(burst) || burst < 1) return setError('Burst time must be a whole number of at least 1.');
  if (!Number.isInteger(priority)) return setError('Priority must be a whole number.');
  if (!Number.isInteger(delay) || delay < 0) return setError('"Arrives in" must be a whole number of 0 or more.');

  const wasPlaying = playing;
  pause();                            
  const now = T;
  const arrival = liveArrival();
  addRow(nextPid(), arrival, burst, priority);
  const row = inputBody.lastElementChild;
  const pid = row.cells[0].firstChild.value;
  try {
    const { procs, error } = readProcesses();
    if (error) throw new Error(error);
    const r = await request(build(lastRun.algo, procs, lastRun.quantum, lastRun.preemptive));
    render(r, now);
    $('liveMsg').textContent = 'Added P' + pid + ', arriving at clock ' + arrival + '. Everything before that is unchanged.';
  } catch (e) {
    row.remove();
    setError(e instanceof TypeError ? UNREACHABLE : e.message);
  }
  if (wasPlaying) play();
}

$('liveBtn').onclick = addLive;
$('liveDelay').oninput = updateLiveNote;

function countUp(id, value, decimals, suffix) {
  const node = $(id);
  const token = (node._tok = (node._tok || 0) + 1);
  const show = v => { node.textContent = v.toFixed(decimals) + suffix; };
  if (matchMedia('(prefers-reduced-motion: reduce)').matches) return show(value);
  const t0 = performance.now();
  (function step(now) {
    if (node._tok !== token) return;
    const k = Math.min(1, (now - t0) / 650);
    show(value * (1 - Math.pow(1 - k, 3)));
    if (k < 1) requestAnimationFrame(step);
  })(t0);
}

function showMetrics(r) {
  countUp('kWT', r.averageWaiting, 2, '');
  countUp('kTAT', r.averageTurnaround, 2, '');
  countUp('kRT', r.averageResponse, 2, '');
  countUp('kUtil', r.totalTime ? (r.totalTime - r.idleTime) / r.totalTime * 100 : 0, 1, '%');
}

const ALGO_COLOR = { rr: 'var(--c-rr)', priority: 'var(--c-pr)' };
const algoName = r => (r.algorithm === 'rr' ? 'RR' : 'Priority');

function dot(r) {
  const d = el('span', 'dot');
  d.style.background = ALGO_COLOR[r.algorithm];
  return d;
}

function fillBars(id, results, key) {
  const box = $(id);
  box.textContent = '';
  const max = Math.max(...results.map(r => r[key]), 1e-9);
  const min = Math.min(...results.map(r => r[key]));
  results.forEach(r => {
    const row = el('div', 'cmp-row'), top = el('div', 'cmp-row-top'), val = el('span', 'cmp-val');
    const track = el('div', 'cmp-track'), fill = el('div', 'cmp-fill');
    top.appendChild(el('span', '', algoName(r)));
    if (r[key] - min < 1e-9) val.appendChild(el('span', 'badge', 'Best'));
    val.appendChild(el('span', '', r[key].toFixed(2)));
    top.appendChild(val);
    fill.style.background = ALGO_COLOR[r.algorithm];
    track.appendChild(fill);
    row.append(top, track);
    box.appendChild(row);
    requestAnimationFrame(() => requestAnimationFrame(() => { fill.style.width = (r[key] / max * 100) + '%'; }));
  });
}

function openCompare(results) {
  const ranked = results.slice().sort((a, b) =>
    a.averageWaiting - b.averageWaiting || a.averageTurnaround - b.averageTurnaround);
  fillBars('barsWT', results, 'averageWaiting');
  fillBars('barsTAT', results, 'averageTurnaround');

  const bestWT = ranked[0].averageWaiting;
  const bestTAT = Math.min(...results.map(r => r.averageTurnaround));
  const body = $('cmpTable').tBodies[0];
  body.textContent = '';
  ranked.forEach((r, i) => {
    const tr = el('tr');
    const name = el('td', '', algoName(r));
    name.prepend(dot(r));
    const wt = el('td', r.averageWaiting - bestWT < 1e-9 ? 'best' : '', r.averageWaiting.toFixed(2));
    const tat = el('td', r.averageTurnaround - bestTAT < 1e-9 ? 'best' : '', r.averageTurnaround.toFixed(2));
    const rt = el('td', '', r.averageResponse.toFixed(2));
    const rank = el('td');
    rank.appendChild(el('span', 'rank' + (i === 0 ? ' first' : ''), '#' + (i + 1)));
    tr.append(name, wt, tat, rt, rank);
    body.appendChild(tr);
  });

  const rr = results.find(r => r.algorithm === 'rr'), pr = results.find(r => r.algorithm === 'priority');
  $('cmpNote').textContent = 'Ranked by average waiting time. Round Robin uses time quantum ' + rr.quantum +
    '. Priority is ' + (pr.preemptive ? 'preemptive' : 'non-preemptive') + '.';
  $('modal').hidden = false;
  $('modalClose').focus();
}

const closeModal = () => { $('modal').hidden = true; };
$('modalClose').onclick = closeModal;
$('modal').onclick = e => { if (e.target === $('modal')) closeModal(); };
document.addEventListener('keydown', e => { if (e.key === 'Escape') closeModal(); });

function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  $('theme').textContent = theme === 'dark' ? '☀️ Light' : '🌙 Dark';
  try { localStorage.setItem('theme', theme); } catch (e) {}
}
applyTheme(document.documentElement.dataset.theme === 'dark' ? 'dark' : 'light');
$('theme').onclick = () => applyTheme(document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark');

if (API) fetch(API + '/health').catch(() => {});