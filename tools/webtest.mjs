// tools/webtest.mjs — @kama/window's browser leg through the driver (AGENTS.window.md item 8, decision 10, and item 10):
// the tests program, built for the web with its own page, run in a headless Chromium driven over the DevTools protocol,
// which passes `--driver` (`?arg=--driver`), sets a device scale of 1.5 and performs trusted input. node 22 or later,
// for its built-in WebSocket; no npm packages.
//
// STOPGAP: every other browser runs the leg under `kama run --target WASM` (tools/test.sh), skipping what needs this
// driver. It stays for trusted input until `kama run` gives it (kama's KR-378), for a device scale (kyoten's KY-52), and
// for the pinned Chrome for Testing, which never loads a page under `kama run` (kyoten's KY-53).
//
//   node tools/webtest.mjs <dir> <page.html[?query]> [--browser <path>] [--timeout <seconds>]
//   node tools/webtest.mjs --list         # the browsers it runs in (tools/browsers.mjs driver), or exit 3 when none
//
// It serves <dir> on 127.0.0.1, opens <page.html> in a throwaway profile at a device scale of 1.5 (so pixel sizes are
// not CSS sizes), and prints the browser's name and the page's console as it comes. The run ends at the program's summary line
// ("window tests: N cases, M failed"), at an abort or an exception, or at the time limit; the exit status is 0 only for
// a summary with no failures.
//
// Trusted input. A case that needs the browser's own behaviour (a real click, pointer lock, a touch the browser may
// take) prints a request and waits for it to be done:
//
//   webtest: <seq> press|release|click <selector> <x> <y> [left|middle|right]    a mouse button, at x, y CSS pixels
//                                                                                 from the element's content box
//   webtest: <seq> move <selector> <x> <y> [buttons]                              the mouse moved there
//   webtest: <seq> key down|up|tap <code> <key> <keyCode>                        a key, to the focused element
//   webtest: <seq> touch <selector> <x0> <y0> <x1> <y1> <steps>                  a finger dragged from one point to another
//   webtest: <seq> wait <ms>                                                     time passing
//
// When it is done the driver sets `globalThis.windowtestsAck` to <seq>, which the program reads.
import { spawn, spawnSync } from 'node:child_process';
import http from 'node:http';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { driverBrowsers } from './browsers.mjs';

const SCALE = 1.5;

const TYPES = {
    '.html': 'text/html', '.js': 'text/javascript', '.wasm': 'application/wasm', '.data': 'application/octet-stream',
    '.json': 'application/json', '.css': 'text/css',
};

function serve(root) {
    const base = path.resolve(root);
    const server = http.createServer((req, res) => {
        const u = new URL(req.url, 'http://x');
        const p = path.resolve(path.join(base, decodeURIComponent(u.pathname)));
        if (!p.startsWith(base)) { res.writeHead(403); res.end(); return; }
        fs.readFile(p, (err, data) => {
            if (err) { res.writeHead(404); res.end('not found'); return; }
            res.writeHead(200, { 'Content-Type': TYPES[path.extname(p)] || 'application/octet-stream', 'Cache-Control': 'no-store' });
            res.end(data);
        });
    });
    return new Promise(r => server.listen(0, '127.0.0.1', () => r({ server, port: server.address().port })));
}

// One DevTools-protocol connection: requests by id, events to the listeners.
class Cdp {
    constructor(ws) {
        this.ws = ws; this.next = 0; this.pending = new Map(); this.listeners = [];
        ws.onmessage = (m) => {
            const msg = JSON.parse(m.data);
            if (msg.id !== undefined) {
                const p = this.pending.get(msg.id);
                if (!p) return;
                this.pending.delete(msg.id);
                if (msg.error) p.reject(new Error(`${p.method}: ${JSON.stringify(msg.error)}`)); else p.resolve(msg.result);
            } else {
                for (const l of this.listeners) l(msg);
            }
        };
    }
    send(method, params = {}, sessionId) {
        const id = ++this.next;
        const msg = { id, method, params };
        if (sessionId) msg.sessionId = sessionId;
        this.ws.send(JSON.stringify(msg));
        return new Promise((resolve, reject) => this.pending.set(id, { resolve, reject, method }));
    }
}

async function launch(browser, profile) {
    fs.rmSync(profile, { recursive: true, force: true });
    const args = ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${profile}`, '--no-first-run',
                  '--no-default-browser-check', '--disable-extensions', `--force-device-scale-factor=${SCALE}`, '--window-size=1280,1024',
                  '--enable-unsafe-webgpu', 'about:blank'];
    const proc = spawn(browser, args, { stdio: ['ignore', 'ignore', 'pipe'] });
    const url = await new Promise((resolve, reject) => {
        let buf = '';
        const t = setTimeout(() => reject(new Error('the browser printed no DevTools address: ' + buf)), 30000);
        proc.stderr.on('data', d => {
            buf += d;
            const m = buf.match(/DevTools listening on (ws:\/\/\S+)/);
            if (m) { clearTimeout(t); resolve(m[1]); }
        });
        proc.on('exit', c => { clearTimeout(t); reject(new Error(`the browser exited (${c}): ${buf}`)); });
        proc.on('error', e => { clearTimeout(t); reject(new Error(`the browser did not start: ${e.message}`)); });
    });
    const ws = new WebSocket(url);
    await new Promise((r, j) => { ws.onopen = r; ws.onerror = j; });
    return { cdp: new Cdp(ws), proc };
}

const sleep = (ms) => new Promise(r => setTimeout(r, ms));

async function main(argv) {
    if (argv[0] === '--list') {
        const found = driverBrowsers();
        if (!found.length) { console.error('webtest: no Chrome for Testing pinned under ~/browsers (or set KAMA_BROWSER)'); return 3; }
        for (const b of found) console.log(b);
        return 0;
    }
    const [dir, page] = argv;
    let limit = 120;
    const ti = argv.indexOf('--timeout');
    if (ti >= 0) limit = Number(argv[ti + 1]);
    const bi = argv.indexOf('--browser');
    if (!dir || !page) { console.error('usage: node tools/webtest.mjs <dir> <page.html> [--browser <path>] [--timeout <seconds>]'); return 2; }
    const browserPath = bi >= 0 ? argv[bi + 1] : driverBrowsers()[0];
    if (!browserPath) { console.error('webtest: no Chrome for Testing pinned under ~/browsers (or set KAMA_BROWSER)'); return 3; }

    const { server, port } = await serve(dir);
    const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'kama-window-webtest-'));
    let launched;
    try {
        launched = await launch(browserPath, profile);
    } catch (e) {
        console.log(`webtest: ${e.message}`);
        server.close();
        try { fs.rmSync(profile, { recursive: true, force: true }); } catch (e2) { }
        return 1;
    }
    const { cdp, proc } = launched;
    let verdict = null;   // the exit status, once the run has ended
    let finish;
    const ended = new Promise(r => { finish = r; });
    const end = (status, why) => { if (verdict === null) { verdict = status; if (why) console.log(why); finish(); } };

    try {
        const version = await cdp.send('Browser.getVersion');
        console.log(`webtest: ${version.product} (${browserPath})`);
        const { targetId } = await cdp.send('Target.createTarget', { url: 'about:blank' });
        const { sessionId } = await cdp.send('Target.attachToTarget', { targetId, flatten: true });
        const send = (method, params = {}) => cdp.send(method, params, sessionId);
        const evaluate = async (expression) => {
            const r = await send('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true });
            if (r.exceptionDetails) throw new Error(r.exceptionDetails.exception?.description || r.exceptionDetails.text);
            return r.result.value;
        };

        // The top-left corner of an element's content box, inside its border and padding, in the page's CSS pixels: where
        // a window's positions start.
        const corner = async (selector) => {
            const r = await evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)});
                if (!e) return null; const b = e.getBoundingClientRect(), cs = getComputedStyle(e);
                const px = (v) => parseFloat(v) || 0;
                return { x: b.left + px(cs.borderLeftWidth) + px(cs.paddingLeft),
                         y: b.top + px(cs.borderTopWidth) + px(cs.paddingTop) }; })()`);
            if (!r) throw new Error(`no element matches ${selector}`);
            return r;
        };
        const BUTTON_BITS = { left: 1, right: 2, middle: 4 };
        let mouseButtons = 0;
        // A move while a button is held names that button, as a drag does (Chromium reads a move's `button` for the
        // drag; with none, the pointer is not in its active buttons state, and pointer capture does not hold).
        const mouse = async (type, x, y, button = 'none') => {
            const bit = BUTTON_BITS[button] || 0;
            if (type === 'mousePressed') mouseButtons |= bit;
            if (type === 'mouseReleased') mouseButtons &= ~bit;
            if (type === 'mouseMoved') {
                button = (mouseButtons & 1) ? 'left' : (mouseButtons & 2) ? 'right' : (mouseButtons & 4) ? 'middle' : 'none';
            }
            await send('Input.dispatchMouseEvent', { type, x, y, button, buttons: mouseButtons, clickCount: type === 'mouseMoved' ? 0 : 1 });
        };
        const perform = async (verb, args) => {
            switch (verb) {
                case 'press': case 'release': case 'click': {
                    const c = await corner(args[0]);
                    const x = c.x + Number(args[1]), y = c.y + Number(args[2]), button = args[3] || 'left';
                    if (verb !== 'release') await mouse('mousePressed', x, y, button);
                    if (verb !== 'press') await mouse('mouseReleased', x, y, button);
                    return;
                }
                case 'move': {
                    const c = await corner(args[0]);
                    await mouse('mouseMoved', c.x + Number(args[1]), c.y + Number(args[2]));
                    return;
                }
                case 'key': {
                    const [how, code, key, keyCode] = args;
                    const vk = Number(keyCode);
                    const params = { code, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk };
                    if (key.length === 1) params.text = key;
                    if (how === 'down' || how === 'tap') await send('Input.dispatchKeyEvent', { type: key.length === 1 ? 'keyDown' : 'rawKeyDown', ...params });
                    if (how === 'up' || how === 'tap') await send('Input.dispatchKeyEvent', { type: 'keyUp', code, key, windowsVirtualKeyCode: vk, nativeVirtualKeyCode: vk });
                    return;
                }
                case 'touch': {
                    const c = await corner(args[0]);
                    const [x0, y0, x1, y1] = args.slice(1, 5).map(Number);
                    const steps = Math.max(1, Number(args[5] || 4));
                    const at = (i) => [{ x: c.x + x0 + (x1 - x0) * i / steps, y: c.y + y0 + (y1 - y0) * i / steps, id: 1, radiusX: 5, radiusY: 5, force: 0.5 }];
                    await send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: at(0) });
                    for (let i = 1; i <= steps; i++) await send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: at(i) });
                    await send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
                    return;
                }
                case 'wait': await sleep(Number(args[0])); return;
                default: throw new Error(`unknown request ${verb}`);
            }
        };

        // Requests are done one at a time, in order, while the console goes on being read.
        let queue = Promise.resolve();
        const request = (line) => {
            const [seq, verb, ...args] = line.trim().split(/\s+/);
            queue = queue.then(async () => {
                if (verdict !== null) return;
                try {
                    await perform(verb, args);
                    await evaluate(`globalThis.windowtestsAck = ${Number(seq)}`);
                } catch (e) {
                    end(1, `webtest: the request "${line.trim()}" failed: ${e.message}`);
                }
            });
        };

        cdp.listeners.push(msg => {
            if (msg.sessionId !== sessionId || verdict !== null) return;
            if (msg.method === 'Runtime.consoleAPICalled') {
                const text = msg.params.args.map(a => a.value !== undefined ? String(a.value) : (a.description || '')).join(' ');
                for (const line of text.split('\n')) {
                    if (line.startsWith('webtest: ')) { request(line.slice('webtest: '.length)); continue; }
                    console.log(line);
                    const m = line.match(/^window tests: (\d+) cases, (\d+) failed/);
                    if (m) end(Number(m[2]) === 0 ? 0 : 1);
                    else if (/^Aborted\(/.test(line)) end(1, 'webtest: the program aborted');
                }
            } else if (msg.method === 'Runtime.exceptionThrown') {
                const d = msg.params.exceptionDetails;
                end(1, `webtest: an exception: ${d.exception?.description || d.text}`);
            }
        });
        await send('Runtime.enable');
        await send('Page.enable');
        await send('Page.navigate', { url: `http://127.0.0.1:${port}/${page}` });
        const timer = setTimeout(() => end(1, `webtest: no summary line after ${limit} s`), limit * 1000);
        await ended;
        clearTimeout(timer);
    } catch (e) {
        end(1, `webtest: ${e.message}`);
    } finally {
        try { await cdp.send('Browser.close'); } catch (e) { }
        await new Promise(r => { if (proc.exitCode !== null) r(); else { proc.on('exit', r); setTimeout(r, 5000); } });
        server.close();
        // The browser's helper processes may hold the profile a moment after it exits; a profile left behind in the
        // system's temporary folder is said, and is not the run's verdict.
        try { fs.rmSync(profile, { recursive: true, force: true, maxRetries: 20, retryDelay: 250 }); }
        catch (e) { console.log(`webtest: the throwaway profile ${profile} is left behind: ${e.message}`); }
    }
    return verdict;
}

process.exitCode = await main(process.argv.slice(2));
