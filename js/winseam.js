// js/winseam.js: @kama/window's web half, linked into a program built for the web (`jsLibraries`). A window is a canvas
// of the page's (AGENTS.window.md item 8). Its listeners never call into the program: each queues a record, and
// `pump()` pulls the queues into the C ring (`winseam_web_pull`), as GLFW's callbacks run inside `glfwPollEvents`, so the
// ring's rules hold here unchanged and nothing a listener does can reach a program that has quit (decision 1). It is
// kama's own `std::net::web` model.
//
// A record is written in `winseam_event`'s layout (csrc/winseam.h), whose offsets winseam.c asserts. Two kinds are the
// web's own and never reach the ring: SIZES, which the C side turns into `Resize` and `ScaleChanged` when they change,
// and the queue's own count of what it dropped, a `Dropped` record.
//
// Each window's queue keeps the ring's promises (decision 1): one pointer's moves coalesce, scrolls of one unit sum, the
// sizes keep the last, and past CAP records an event is counted into a `Dropped` record where it happened, except an up,
// a cancel, a focus change, a pointer lock and the sizes, which go on to HARD so a key is never left held.

mergeInto(LibraryManager.library, {
  $WinSeam__deps: ['$UTF8ToString'],
  $WinSeam: {
    windows: [],            // open windows by index, null once closed
    canvases: new Set(),    // the canvases that are windows
    CAP: 1024,
    HARD: 1536,

    KEY_DOWN: 1, KEY_UP: 2, TEXT: 3, POINTER_MOVE: 4, POINTER_DOWN: 5, POINTER_UP: 6, POINTER_CANCEL: 7,
    POINTER_ENTER: 8, POINTER_LEAVE: 9, SCROLL: 10, FOCUS: 13, POINTER_LOCK: 14, DROPPED: 16, SIZES: 100,

    // The DOM's `KeyboardEvent.code` to the USB HID usage id `Key` is numbered by: every key `Key` names. A code not
    // here is `Key::Unknown` (0), whose default the window leaves to the page.
    codes: {
      KeyA: 4, KeyB: 5, KeyC: 6, KeyD: 7, KeyE: 8, KeyF: 9, KeyG: 10, KeyH: 11, KeyI: 12, KeyJ: 13, KeyK: 14, KeyL: 15,
      KeyM: 16, KeyN: 17, KeyO: 18, KeyP: 19, KeyQ: 20, KeyR: 21, KeyS: 22, KeyT: 23, KeyU: 24, KeyV: 25, KeyW: 26,
      KeyX: 27, KeyY: 28, KeyZ: 29,
      Digit1: 30, Digit2: 31, Digit3: 32, Digit4: 33, Digit5: 34, Digit6: 35, Digit7: 36, Digit8: 37, Digit9: 38,
      Digit0: 39,
      Enter: 40, Escape: 41, Backspace: 42, Tab: 43, Space: 44, Minus: 45, Equal: 46, BracketLeft: 47, BracketRight: 48,
      Backslash: 49, IntlHash: 50, Semicolon: 51, Quote: 52, Backquote: 53, Comma: 54, Period: 55, Slash: 56,
      CapsLock: 57,
      F1: 58, F2: 59, F3: 60, F4: 61, F5: 62, F6: 63, F7: 64, F8: 65, F9: 66, F10: 67, F11: 68, F12: 69,
      PrintScreen: 70, ScrollLock: 71, Pause: 72, Insert: 73, Home: 74, PageUp: 75, Delete: 76, End: 77, PageDown: 78,
      ArrowRight: 79, ArrowLeft: 80, ArrowDown: 81, ArrowUp: 82,
      NumLock: 83, NumpadDivide: 84, NumpadMultiply: 85, NumpadSubtract: 86, NumpadAdd: 87, NumpadEnter: 88,
      Numpad1: 89, Numpad2: 90, Numpad3: 91, Numpad4: 92, Numpad5: 93, Numpad6: 94, Numpad7: 95, Numpad8: 96,
      Numpad9: 97, Numpad0: 98, NumpadDecimal: 99, IntlBackslash: 100, ContextMenu: 101, NumpadEqual: 103,
      F13: 104, F14: 105, F15: 106, F16: 107, F17: 108, F18: 109, F19: 110, F20: 111, F21: 112, F22: 113, F23: 114,
      F24: 115,
      ControlLeft: 224, ShiftLeft: 225, AltLeft: 226, MetaLeft: 227, ControlRight: 228, ShiftRight: 229, AltRight: 230,
      MetaRight: 231,
    },

    // A W3C `button` to its bit in `buttons`, which numbers them otherwise (right is 2, middle 4).
    buttonBits: [1, 4, 2, 8, 16, 32],

    mods: function (e) {
      var m = 0;
      if (e.shiftKey) m |= 1;
      if (e.ctrlKey) m |= 2;
      if (e.altKey) m |= 4;
      if (e.metaKey) m |= 8;
      if (e.getModifierState && e.getModifierState('CapsLock')) m |= 16;
      if (e.getModifierState && e.getModifierState('NumLock')) m |= 32;
      return m;
    },

    // The codepoint a keydown types, or 0: its `key` when that is one printable character, typed with neither Control
    // nor Meta unless AltGraph made it (so Ctrl+S types nothing, and AltGr+E types €). A dead key and an IME's are not
    // text here (WIN-5).
    typed: function (e) {
      var k = e.key;
      if (typeof k !== 'string' || k.length === 0 || k.length > 2) return 0;
      var cp = k.codePointAt(0);
      if (String.fromCodePoint(cp) !== k) return 0;
      if (cp < 32 || cp === 127) return 0;
      var altGraph = e.getModifierState && e.getModifierState('AltGraph');
      if ((e.ctrlKey || e.metaKey) && !altGraph) return 0;
      return cp;
    },

    pointerKind: function (type) { return type === 'touch' ? 1 : type === 'pen' ? 2 : 0; },

    // A window on `canvas`, its listeners attached. `record` is the C record its events go to.
    open: function (record, canvas, width, height, focused, sizedByPage) {
      var S = WinSeam;
      var win = {
        record: record, canvas: canvas, queue: [], head: 0, listeners: [], last: new Map(),
        mode: 0, lockWanted: false, locked: false, insets: { left: 0, top: 0, right: 0, bottom: 0 },
        sized: { w: 0, h: 0, fw: 0, fh: 0, scale: 0 }, observer: null, media: null,
        saved: {
          width: canvas.style.width, height: canvas.style.height, touchAction: canvas.style.touchAction,
          cursor: canvas.style.cursor, tabindex: canvas.getAttribute('tabindex'), sizedByPage: sizedByPage,
        },
      };
      if (!sizedByPage) {
        canvas.style.width = width + 'px';
        canvas.style.height = height + 'px';
      }
      canvas.style.touchAction = 'none';
      if (win.saved.tabindex === null) canvas.setAttribute('tabindex', '0');
      if (focused) canvas.focus({ preventScroll: true });   // before the listeners: no Focus at open, as natively
      S.measure(win, false);

      var on = function (target, type, fn, options) {
        target.addEventListener(type, fn, options);
        win.listeners.push([target, type, fn, options]);
      };
      on(canvas, 'keydown', function (e) { S.key(win, e, true); });
      on(canvas, 'keyup', function (e) { S.key(win, e, false); });
      on(canvas, 'focus', function () { S.put(win, { kind: S.FOCUS, a: 1 }); });
      on(canvas, 'blur', function () { S.put(win, { kind: S.FOCUS, a: 0 }); });
      on(canvas, 'pointerdown', function (e) { S.pointerDown(win, e); });
      on(canvas, 'pointermove', function (e) { S.pointerMove(win, e); });
      on(canvas, 'pointerup', function (e) { S.pointerButton(win, e, S.POINTER_UP, e.button); });
      on(canvas, 'pointercancel', function (e) { S.put(win, S.pointer(win, e, S.POINTER_CANCEL)); });
      on(canvas, 'pointerenter', function (e) { S.put(win, S.pointer(win, e, S.POINTER_ENTER)); });
      on(canvas, 'pointerleave', function (e) {
        S.put(win, S.pointer(win, e, S.POINTER_LEAVE));
        if (e.pointerType !== 'mouse') win.last.delete(e.pointerId);
      });
      on(canvas, 'wheel', function (e) { S.wheel(win, e); }, { passive: false });
      on(canvas, 'contextmenu', function (e) { e.preventDefault(); });
      on(document, 'pointerlockchange', function () { S.lockChanged(win); });

      if (typeof ResizeObserver !== 'undefined') {
        win.observer = new ResizeObserver(function (entries) { S.observed(win, entries[entries.length - 1]); });
        try { win.observer.observe(canvas, { box: 'device-pixel-content-box' }); }
        catch (e) { win.observer.observe(canvas); }
      }
      S.watchScale(win);
      var index = S.windows.length;
      S.windows.push(win);
      S.canvases.add(canvas);
      return index;
    },

    close: function (index) {
      var S = WinSeam, win = S.windows[index];
      if (!win) return;
      for (var i = 0; i < win.listeners.length; i++) {
        var l = win.listeners[i];
        l[0].removeEventListener(l[1], l[2], l[3]);
      }
      if (win.observer) win.observer.disconnect();
      if (win.media) win.media.list.removeEventListener('change', win.media.fn);
      var c = win.canvas;
      if (document.pointerLockElement === c) document.exitPointerLock();
      if (!win.saved.sizedByPage) { c.style.width = win.saved.width; c.style.height = win.saved.height; }
      c.style.touchAction = win.saved.touchAction;
      c.style.cursor = win.saved.cursor;
      if (win.saved.tabindex === null) c.removeAttribute('tabindex');
      S.canvases.delete(c);
      S.windows[index] = null;
    },

    // ---- the queue ------------------------------------------------------------------------------------------------

    put: function (win, r) {
      var S = WinSeam, q = win.queue, n = q.length - win.head;
      var last = n > 0 ? q[q.length - 1] : null;
      if (last && last.kind === r.kind) {
        if (r.kind === S.POINTER_MOVE && last.pointer === r.pointer && last.pointerKind === r.pointerKind) {
          var dx = last.dx + r.dx, dy = last.dy + r.dy;
          q[q.length - 1] = r;
          r.dx = dx; r.dy = dy;
          return;
        }
        if (r.kind === S.SCROLL && last.a === r.a) { last.dx += r.dx; last.dy += r.dy; return; }
        if (r.kind === S.SIZES) { q[q.length - 1] = r; return; }
      }
      var owed = r.kind === S.KEY_UP || r.kind === S.POINTER_UP || r.kind === S.POINTER_CANCEL || r.kind === S.FOCUS ||
                 r.kind === S.POINTER_LOCK || r.kind === S.SIZES;
      if (n < S.CAP || (owed && n < S.HARD)) { q.push(r); return; }
      if (last && last.kind === S.DROPPED) { last.a += 1; return; }
      q.push({ kind: S.DROPPED, a: 1 });
    },

    // The next record of any window into `into`, oldest first, window by window; its C record, or 0 when none is left.
    pull: function (into) {
      var S = WinSeam;
      for (var i = 0; i < S.windows.length; i++) {
        var win = S.windows[i];
        if (!win || win.head >= win.queue.length) continue;
        var r = win.queue[win.head];
        win.head += 1;
        if (win.head >= win.queue.length) { win.queue = []; win.head = 0; }
        S.write(into, r);
        return win.record;
      }
      return 0;
    },

    // `winseam_event`'s layout: four int32, four float64, four int32, six float32 (88 bytes; winseam.c asserts it).
    write: function (p, r) {
      HEAP32[p >> 2] = r.kind;
      HEAP32[(p >> 2) + 1] = r.a | 0;
      HEAP32[(p >> 2) + 2] = r.b | 0;
      HEAP32[(p >> 2) + 3] = r.c | 0;
      HEAPF64[(p + 16) >> 3] = r.x || 0;
      HEAPF64[(p + 24) >> 3] = r.y || 0;
      HEAPF64[(p + 32) >> 3] = r.dx || 0;
      HEAPF64[(p + 40) >> 3] = r.dy || 0;
      HEAP32[(p + 48) >> 2] = r.pointer | 0;
      HEAP32[(p + 52) >> 2] = r.pointerKind | 0;
      HEAP32[(p + 56) >> 2] = r.primary | 0;
      HEAP32[(p + 60) >> 2] = 0;   // buttons: the window's to say
      HEAPF32[(p + 64) >> 2] = r.pressure || 0;
      HEAPF32[(p + 68) >> 2] = r.tiltX || 0;
      HEAPF32[(p + 72) >> 2] = r.tiltY || 0;
      HEAPF32[(p + 76) >> 2] = r.twist || 0;
      HEAPF32[(p + 80) >> 2] = r.width || 0;
      HEAPF32[(p + 84) >> 2] = r.height || 0;
    },

    // ---- keys -----------------------------------------------------------------------------------------------------

    // A key the window names has its default prevented (decision 5): Space does not scroll, Tab does not leave, F5 does
    // not reload. `Key::Unknown` is left to the page. Text comes from the keydown's `key`, after the key.
    key: function (win, e, down) {
      var S = WinSeam, hid = S.codes[e.code] || 0;
      if (hid !== 0) e.preventDefault();
      S.put(win, { kind: down ? S.KEY_DOWN : S.KEY_UP, a: hid, b: S.mods(e), c: down && e.repeat ? 1 : 0 });
      if (!down) return;
      var cp = S.typed(e);
      if (cp !== 0) S.put(win, { kind: S.TEXT, a: cp });
    },

    // ---- pointers -------------------------------------------------------------------------------------------------

    // A pointer event as a record, at its position in the canvas's content box. While locked the browser holds the
    // position still (the Pointer Lock spec; measured in Edge), and the motion is in `movementX` and `movementY`.
    pointer: function (win, e, kind) {
      var S = WinSeam, c = win.canvas, b = c.getBoundingClientRect();
      var x = e.clientX - b.left - win.insets.left, y = e.clientY - b.top - win.insets.top;
      var r = {
        kind: kind, b: S.mods(e), x: x, y: y, dx: 0, dy: 0, pointer: e.pointerId,
        pointerKind: S.pointerKind(e.pointerType), primary: e.isPrimary ? 1 : 0, pressure: e.pressure,
        tiltX: e.tiltX, tiltY: e.tiltY, twist: e.twist, width: e.width, height: e.height,
      };
      win.last.set(e.pointerId, { x: x, y: y });
      return r;
    },

    pointerButton: function (win, e, kind, button) {
      var S = WinSeam, r = S.pointer(win, e, kind);
      r.a = button;
      S.put(win, r);
    },

    // A down: the window takes it from the page (no selection, no drag of the image), focuses the canvas itself, keeps
    // the pointer's events until its up wherever it goes (as a native window captures the mouse), and asks for the lock
    // a program wished for, in the gesture the browser requires (decision 7).
    pointerDown: function (win, e) {
      var S = WinSeam, c = win.canvas;
      e.preventDefault();
      if (document.activeElement !== c) c.focus({ preventScroll: true });
      try { c.setPointerCapture(e.pointerId); } catch (err) { }
      if (win.lockWanted && document.pointerLockElement !== c) S.requestLock(win);
      S.pointerButton(win, e, S.POINTER_DOWN, e.button);
    },

    // A move, or a second button pressed or released while one is held, which the W3C sends as a move with `button` set
    // (chorded buttons): it becomes that button's own down or up (decision 2.2).
    pointerMove: function (win, e) {
      var S = WinSeam, c = win.canvas, before = win.last.get(e.pointerId);
      if (e.button >= 0 && e.button < S.buttonBits.length) {
        var r = S.pointer(win, e, (e.buttons & S.buttonBits[e.button]) !== 0 ? S.POINTER_DOWN : S.POINTER_UP);
        r.a = e.button;
        if (before && (before.x !== r.x || before.y !== r.y)) {
          var m = S.pointer(win, e, S.POINTER_MOVE);
          m.dx = m.x - before.x; m.dy = m.y - before.y;
          S.put(win, m);
        }
        S.put(win, r);
        return;
      }
      var r = S.pointer(win, e, S.POINTER_MOVE);
      if (document.pointerLockElement === c) { r.dx = e.movementX; r.dy = e.movementY; }
      else if (before) { r.dx = r.x - before.x; r.dy = r.y - before.y; }
      S.put(win, r);
    },

    // A wheel over the canvas is the program's, not the page's. Its unit is the browser's (decision 9): lines, or pixels,
    // and a page's worth is the canvas's height in pixels. Positive is left and up, as GLFW and winit report, so the
    // DOM's deltas are negated.
    wheel: function (win, e) {
      var S = WinSeam;
      e.preventDefault();
      var unit = e.deltaMode === 1 ? 0 : 1, k = e.deltaMode === 2 ? (win.sized.h || 1) : 1;
      S.put(win, { kind: S.SCROLL, a: unit, dx: -e.deltaX * k, dy: -e.deltaY * k });
    },

    // ---- pointer lock (decision 7) --------------------------------------------------------------------------------

    requestLock: function (win) {
      var c = win.canvas, p = null;
      try { p = c.requestPointerLock({ unadjustedMovement: true }); } catch (err) { p = null; }
      if (p && typeof p.catch === 'function') {
        p.catch(function (err) {
          if (!err || err.name !== 'NotSupportedError') return;   // refused: the wish stands for the next click
          try {
            var q = c.requestPointerLock();
            if (q && typeof q.catch === 'function') q.catch(function () { });
          } catch (e2) { }
        });
      }
    },

    lockChanged: function (win) {
      var S = WinSeam, now = document.pointerLockElement === win.canvas;
      if (now === win.locked) return;
      win.locked = now;
      if (!now) win.lockWanted = false;   // the browser gave it back: the mode falls back to Normal
      S.put(win, { kind: S.POINTER_LOCK, a: now ? 1 : 0 });
    },

    setCursor: function (win, mode) {
      var c = win.canvas;
      win.mode = mode;
      c.style.cursor = mode === 1 ? 'none' : win.saved.cursor;
      if (mode === 2) { win.lockWanted = !win.locked; return; }
      win.lockWanted = false;
      if (win.locked && document.pointerLockElement === c) document.exitPointerLock();
    },

    // ---- sizes (decision 6) ---------------------------------------------------------------------------------------

    // Which axes of the canvas's layout follow its pixels (WIN-21): the pixels changed once, by a step on each axis
    // that also changes their aspect, the layout read, and the pixels put back. A canvas the page gives no CSS size is
    // laid out at its pixel size, so under `sizedByPage`, where the pixels are set from the layout × the pixel ratio,
    // it would grow each frame. One axis following holds: the page sizes the other, and the pixels keep their aspect.
    follows: function (canvas) {
      var w0 = canvas.width, h0 = canvas.height;
      var a = canvas.getBoundingClientRect();
      canvas.width = w0 + 37;
      canvas.height = h0 + 23;
      var b = canvas.getBoundingClientRect();
      canvas.width = w0;
      canvas.height = h0;
      return { width: b.width !== a.width, height: b.height !== a.height };
    },

    // The canvas's content box, its pixels from the CSS size and the device pixel ratio, and the backing store set to
    // them. `report` queues SIZES for the C side, which posts `Resize` and `ScaleChanged` for what changed.
    measure: function (win, report) {
      var c = win.canvas, cs = getComputedStyle(c), b = c.getBoundingClientRect();
      var px = function (v) { return parseFloat(v) || 0; };
      win.insets = {
        left: px(cs.borderLeftWidth) + px(cs.paddingLeft), top: px(cs.borderTopWidth) + px(cs.paddingTop),
        right: px(cs.borderRightWidth) + px(cs.paddingRight), bottom: px(cs.borderBottomWidth) + px(cs.paddingBottom),
      };
      var w = b.width - win.insets.left - win.insets.right, h = b.height - win.insets.top - win.insets.bottom;
      var scale = window.devicePixelRatio || 1;
      WinSeam.sizes(win, w, h, Math.round(w * scale), Math.round(h * scale), scale, report);
    },

    observed: function (win, entry) {
      var css = entry.contentBoxSize && entry.contentBoxSize[0];
      var w = css ? css.inlineSize : entry.contentRect.width, h = css ? css.blockSize : entry.contentRect.height;
      var scale = window.devicePixelRatio || 1;
      var dev = entry.devicePixelContentBoxSize && entry.devicePixelContentBoxSize[0];
      var fw = dev ? dev.inlineSize : Math.round(w * scale), fh = dev ? dev.blockSize : Math.round(h * scale);
      WinSeam.sizes(win, w, h, fw, fh, scale, true);
    },

    sizes: function (win, w, h, fw, fh, scale, report) {
      var c = win.canvas, s = win.sized;
      if (c.width !== fw) c.width = fw;
      if (c.height !== fh) c.height = fh;
      var lw = Math.round(w), lh = Math.round(h);
      if (s.w === lw && s.h === lh && s.fw === fw && s.fh === fh && s.scale === scale) return;
      win.sized = { w: lw, h: lh, fw: fw, fh: fh, scale: scale };
      if (report) WinSeam.put(win, { kind: WinSeam.SIZES, a: lw, b: lh, x: scale, dx: fw, dy: fh });
    },

    // A change of the device pixel ratio (another display, the page's zoom): a `matchMedia` query on the ratio now,
    // asked again at each change.
    watchScale: function (win) {
      if (typeof matchMedia !== 'function') return;
      if (win.media) win.media.list.removeEventListener('change', win.media.fn);
      var list = matchMedia('(resolution: ' + (window.devicePixelRatio || 1) + 'dppx)');
      var fn = function () { WinSeam.measure(win, true); WinSeam.watchScale(win); };
      list.addEventListener('change', fn);
      win.media = { list: list, fn: fn };
    },
  },

  // ---- what C calls ---------------------------------------------------------------------------------------------------

  // A window on the canvas `selector` names: its index, or -1 with no page (node), -2 when nothing matches, -3 when the
  // element is not a canvas, -4 when the canvas is already a window, -5 under `sizedByPage` when the page sizes the
  // canvas on neither axis (WIN-21), which is checked before anything of the canvas is touched. `sizes` gets the CSS
  // width and height, rounded, and the pixel width and height; `scale` the device pixel ratio.
  winseam_web_open__deps: ['$WinSeam', '$UTF8ToString'],
  winseam_web_open: function (record, selector, width, height, focused, sizedByPage, sizes, scale) {
    if (typeof document === 'undefined') return -1;
    var el = null;
    try { el = document.querySelector(UTF8ToString(selector)); } catch (err) { el = null; }
    if (!el) return -2;
    if (!(el instanceof HTMLCanvasElement)) return -3;
    if (WinSeam.canvases.has(el)) return -4;
    if (sizedByPage !== 0) {
      var f = WinSeam.follows(el);
      if (f.width && f.height) return -5;
    }
    var index = WinSeam.open(record, el, width, height, focused !== 0, sizedByPage !== 0);
    var s = WinSeam.windows[index].sized;
    HEAP32[sizes >> 2] = s.w;
    HEAP32[(sizes >> 2) + 1] = s.h;
    HEAP32[(sizes >> 2) + 2] = s.fw;
    HEAP32[(sizes >> 2) + 3] = s.fh;
    HEAPF32[scale >> 2] = s.scale;
    return index;
  },

  winseam_web_close__deps: ['$WinSeam'],
  winseam_web_close: function (index) { WinSeam.close(index); },

  winseam_web_pull__deps: ['$WinSeam'],
  winseam_web_pull: function (into) { return WinSeam.pull(into); },

  winseam_web_set_cursor__deps: ['$WinSeam'],
  winseam_web_set_cursor: function (index, mode) {
    var win = WinSeam.windows[index];
    if (win) WinSeam.setCursor(win, mode);
  },

  // The key table, for the tests: a DOM code to its HID id (0 when the window does not name it), and how many it names.
  // WIN-4: the page's pads, by the Gamepad API (decision 1). One read of `navigator.getGamepads()` a pump; each slot is
  // the pad's browser index, the lowest free, reused. A standard-mapped pad fills the standard view, its values as the
  // browser gives them, untouched; every pad fills the raw view, its buttons and axes in the browser's order. Its vendor
  // and product are read from its id, as Chromium ("Vendor: 054c Product: 09cc") or Firefox ("054c-09cc-") writes them;
  // 0 where neither does.
  winseam_web_pads_read__deps: ['$WinSeam'],
  winseam_web_pads_read: function () {
    WinSeam.pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ? navigator.getGamepads() : [];
  },
  winseam_web_pad__deps: ['$WinSeam', '$stringToUTF8'],
  winseam_web_pad: function (slot, sample, name, nameCapacity, meta, rawButtons, rawAxes) {
    var pads = WinSeam.pads || [];
    var p = slot < pads.length ? pads[slot] : null;
    if (!p || !p.connected) return 0;
    var standard = p.mapping === 'standard';
    for (var b = 0; b < 17; b++) {
      var button = standard && b < p.buttons.length ? p.buttons[b] : null;
      HEAPF32[((sample + 4) >> 2) + b] = button ? button.value : 0;
    }
    for (var a = 0; a < 4; a++) HEAPF32[((sample + 72) >> 2) + a] = standard && a < p.axes.length ? p.axes[a] : 0;
    var id = p.id || '';
    stringToUTF8(id, name, nameCapacity);
    var m = /Vendor: ([0-9a-f]{4}) Product: ([0-9a-f]{4})/i.exec(id) || /^([0-9a-f]{1,4})-([0-9a-f]{1,4})-/i.exec(id);
    var caps = 0, actuator = p.vibrationActuator;
    if (actuator) {
      var effects = actuator.effects || (actuator.type ? [actuator.type] : []);
      if (effects.indexOf('dual-rumble') >= 0) caps |= 1;
      if (effects.indexOf('trigger-rumble') >= 0) caps |= 2;
    }
    var nb = Math.min(p.buttons.length, 32), na = Math.min(p.axes.length, 8);
    HEAP32[meta >> 2] = m ? parseInt(m[1], 16) : 0;
    HEAP32[(meta >> 2) + 1] = m ? parseInt(m[2], 16) : 0;
    HEAP32[(meta >> 2) + 2] = caps;
    HEAP32[(meta >> 2) + 3] = standard ? 1 : 0;
    HEAP32[(meta >> 2) + 4] = nb;
    HEAP32[(meta >> 2) + 5] = na;
    for (b = 0; b < nb; b++) HEAPU8[rawButtons + b] = p.buttons[b].pressed ? 1 : 0;
    for (a = 0; a < na; a++) HEAPF32[(rawAxes >> 2) + a] = p.axes[a];
    return 1;
  },
  // A rumble on the page's pad at `slot`: its actuator's dual-rumble or trigger-rumble effect, or its reset to stop.
  // The browser's promise settles later and is not waited on: 1 once asked, 0 with no actuator.
  winseam_web_rumble__deps: ['$WinSeam'],
  winseam_web_rumble: function (slot, a, b, milliseconds, triggers) {
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ? navigator.getGamepads() : [];
    var p = slot < pads.length ? pads[slot] : null;
    var actuator = p ? p.vibrationActuator : null;
    if (!actuator) return 0;
    try {
      var done;
      if (a === 0 && b === 0 && milliseconds === 0 && actuator.reset) done = actuator.reset();
      else if (triggers) done = actuator.playEffect('trigger-rumble', { startDelay: 0, duration: milliseconds, leftTrigger: a, rightTrigger: b });
      else done = actuator.playEffect('dual-rumble', { startDelay: 0, duration: milliseconds, strongMagnitude: a, weakMagnitude: b });
      if (done && done.catch) done.catch(function () {});
      return 1;
    } catch (e) {
      return 0;
    }
  },

  winseam_web_code_to_hid__deps: ['$WinSeam', '$UTF8ToString'],
  winseam_web_code_to_hid: function (code) {
    var c = UTF8ToString(code);
    return Object.prototype.hasOwnProperty.call(WinSeam.codes, c) ? WinSeam.codes[c] : 0;
  },
  winseam_web_code_count__deps: ['$WinSeam'],
  winseam_web_code_count: function () { return Object.keys(WinSeam.codes).length; },
});
