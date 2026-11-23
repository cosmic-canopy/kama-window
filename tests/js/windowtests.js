// tests/js/windowtests.js: what the tests program does to the page, on the web (tests/src/web.kama). It dispatches DOM
// events at an element, the web's equivalent of the Win32 messages natively, and reads back what the window did to it.
// A dispatch answers 1 when the event's default was left to the page and 0 when a listener prevented it.
//
// The gate builds it in a release build too, which runs Closure over it (kama 0.9.966, KR-289): a name the driver or the
// package reads is quoted, as the driver's acknowledgement and the fake pads' Gamepad keys are, and these are the file's
// own, which tools/closure-names.mjs lets Closure rename:
// closure-own: el at kinds sizes windows windowtestsPads windowtestsRecord count motors ms
// closure-own: units winOf typed put TEXT
mergeInto(LibraryManager.library, {
  $WT__deps: ['$UTF8ToString', '$WinSeam'],
  $WT: {
    el: function (p) { return document.querySelector(UTF8ToString(p)); },
    // A string of one or two UTF-16 code units (`lo` 0 for one), which a kama string, UTF-8, cannot carry when a unit is
    // a lone surrogate (WIN-27).
    units: function (hi, lo) { return lo === 0 ? String.fromCharCode(hi) : String.fromCharCode(hi, lo); },
    // The package's window on `el`, or null.
    winOf: function (el) {
      for (var i = 0; i < WinSeam.windows.length; i++) {
        var win = WinSeam.windows[i];
        if (win && win.canvas === el) return win;
      }
      return null;
    },
    // x, y from the element's content box, as the window reports them, to the page's client coordinates.
    at: function (el, x, y) {
      var b = el.getBoundingClientRect(), cs = getComputedStyle(el);
      return {
        clientX: b.left + (parseFloat(cs.borderLeftWidth) || 0) + (parseFloat(cs.paddingLeft) || 0) + x,
        clientY: b.top + (parseFloat(cs.borderTopWidth) || 0) + (parseFloat(cs.paddingTop) || 0) + y,
      };
    },
    kinds: ['mouse', 'touch', 'pen'],
  },

  windowtests_in_browser: function () { return typeof document !== 'undefined' ? 1 : 0; },

  // mods: Shift 1, Control 2, Alt 4, Meta 8, and 64 for AltGraph.
  windowtests_key__deps: ['$WT'],
  windowtests_key: function (sel, type, code, key, mods, repeat) {
    var e = new KeyboardEvent(UTF8ToString(type), {
      code: UTF8ToString(code), key: UTF8ToString(key), repeat: repeat !== 0, shiftKey: (mods & 1) !== 0,
      ctrlKey: (mods & 2) !== 0, altKey: (mods & 4) !== 0, metaKey: (mods & 8) !== 0,
      modifierAltGraph: (mods & 64) !== 0, bubbles: true, cancelable: true, composed: true,
    });
    return WT.el(sel).dispatchEvent(e) ? 1 : 0;
  },

  // WIN-27: a key event whose `key` is `hi` and `lo` as UTF-16 code units, which a page's own event may make a lone
  // surrogate, with no modifier.
  windowtests_key_units__deps: ['$WT', '$UTF8ToString'],
  windowtests_key_units: function (sel, type, code, hi, lo) {
    var e = new KeyboardEvent(UTF8ToString(type), {
      code: UTF8ToString(code), key: WT.units(hi, lo), bubbles: true, cancelable: true, composed: true,
    });
    return WT.el(sel).dispatchEvent(e) ? 1 : 0;
  },

  // WIN-27, the page's own end alone: what the package's `typed` reads from a keydown whose `key` is those code units.
  windowtests_typed__deps: ['$WinSeam', '$WT'],
  windowtests_typed: function (hi, lo) {
    return WinSeam.typed(new KeyboardEvent('keydown', { code: 'KeyA', key: WT.units(hi, lo) }));
  },

  // WIN-27, `eventOf`'s own end alone: a Text record of `value` put into the window on `sel`'s queue as it is, past the
  // page's `typed`. 1 when the canvas is a window.
  windowtests_put_text__deps: ['$WinSeam', '$WT'],
  windowtests_put_text: function (sel, value) {
    var win = WT.winOf(WT.el(sel));
    if (win === null) return 0;
    WinSeam.put(win, { kind: WinSeam.TEXT, a: value, at: performance.now() });
    return 1;
  },

  windowtests_pointer__deps: ['$WT', '$UTF8ToString'],
  windowtests_pointer: function (sel, type, id, kind, x, y, button, buttons, pressure, tiltX, tiltY, twist, width,
                                 height, primary) {
    var el = WT.el(sel), at = WT.at(el, x, y);
    var e = new PointerEvent(UTF8ToString(type), {
      pointerId: id, pointerType: WT.kinds[kind], clientX: at.clientX, clientY: at.clientY, button: button,
      buttons: buttons, pressure: pressure, tiltX: tiltX, tiltY: tiltY, twist: twist, width: width, height: height,
      isPrimary: primary !== 0, bubbles: true, cancelable: true, composed: true,
    });
    return el.dispatchEvent(e) ? 1 : 0;
  },

  windowtests_wheel__deps: ['$WT'],
  windowtests_wheel: function (sel, dx, dy, mode) {
    var e = new WheelEvent('wheel', { deltaX: dx, deltaY: dy, deltaMode: mode, bubbles: true, cancelable: true });
    return WT.el(sel).dispatchEvent(e) ? 1 : 0;
  },

  windowtests_context_menu__deps: ['$WT'],
  windowtests_context_menu: function (sel) {
    return WT.el(sel).dispatchEvent(new MouseEvent('contextmenu', { bubbles: true, cancelable: true })) ? 1 : 0;
  },

  windowtests_focus__deps: ['$WT'],
  windowtests_focus: function (sel) { WT.el(sel).focus({ preventScroll: true }); },

  windowtests_active__deps: ['$WT'],
  windowtests_active: function (sel) { return document.activeElement === WT.el(sel) ? 1 : 0; },

  // 1 when the element's inline style property is `value` ("" when unset).
  windowtests_style_is__deps: ['$WT', '$UTF8ToString'],
  windowtests_style_is: function (sel, prop, value) {
    return WT.el(sel).style[UTF8ToString(prop)] === UTF8ToString(value) ? 1 : 0;
  },

  // 1 when the element's attribute is `value`, "(none)" meaning absent.
  windowtests_attr_is__deps: ['$WT', '$UTF8ToString'],
  windowtests_attr_is: function (sel, name, value) {
    var v = WT.el(sel).getAttribute(UTF8ToString(name));
    return (v === null ? '(none)' : v) === UTF8ToString(value) ? 1 : 0;
  },

  windowtests_set_style__deps: ['$WT', '$UTF8ToString'],
  windowtests_set_style: function (sel, prop, value) { WT.el(sel).style[UTF8ToString(prop)] = UTF8ToString(value); },

  // The canvas's backing store, its pixels.
  windowtests_backing__deps: ['$WT'],
  windowtests_backing: function (sel, width, height) {
    var c = WT.el(sel);
    HEAP32[width >> 2] = c.width;
    HEAP32[height >> 2] = c.height;
  },

  // The last request tools/webtest.mjs has done (its `windowtestsAck`), read by its quoted name: the driver sets it from
  // outside the program, and a release build's Closure renamed the unquoted one, so no step was ever acknowledged.
  windowtests_acked: function () { return (globalThis['windowtestsAck'] | 0); },

  // The page's device scale, which `kama run --scale` and the driver set (1.5 in both of this package's runs).
  windowtests_device_scale: function () { return typeof window !== 'undefined' ? (window.devicePixelRatio || 1) : 1; },

  // A size report handed to the window on `sel` as its ResizeObserver or its `matchMedia` query would make one: the
  // CSS size, the pixels and the device pixel ratio. A real change of the ratio (another display, the page's zoom)
  // cannot be made headless, and the DevTools protocol's emulation did not reach the page's query (2026-11-11), so the
  // tests reach into the package's library for what follows the browser's report. 1 when the canvas is a window.
  windowtests_report_sizes__deps: ['$WinSeam', '$WT'],
  windowtests_report_sizes: function (sel, width, height, fw, fh, scale) {
    var el = WT.el(sel);
    for (var i = 0; i < WinSeam.windows.length; i++) {
      var win = WinSeam.windows[i];
      if (win && win.canvas === el) { WinSeam.sizes(win, width, height, fw, fh, scale, true); return 1; }
    }
    return 0;
  },

  windowtests_locked__deps: ['$WT'],
  windowtests_locked: function (sel) { return document.pointerLockElement === WT.el(sel) ? 1 : 0; },

  // WIN-4: the tests' own pads, in place of the browser's (decision 8). The first call replaces `navigator.getGamepads`
  // with one that lists them, as a browser does, by index; each is a plain object shaped as a Gamepad, its actuator
  // recording what it is asked to play. Its buttons' `pressed` is the browser's own rule, at half their travel, so a
  // trigger the package calls pressed at 30 of 255 can be one the browser does not. Its keys are quoted, so a release
  // build's Closure keeps them, as it keeps a browser object's: the package reads `effects` by its quoted name.
  windowtests_web_pad__deps: ['$UTF8ToString'],
  windowtests_web_pad: function (slot, connected, standard, id, buttons, nbuttons, axes, naxes, effects) {
    if (!globalThis.windowtestsPads) {
      globalThis.windowtestsPads = [];
      Object.defineProperty(navigator, 'getGamepads', {
        configurable: true,
        value: function () { return globalThis.windowtestsPads.slice(); },
      });
    }
    var pads = globalThis.windowtestsPads;
    if (!connected) { pads[slot] = null; return; }
    var bs = [], as = [];
    for (var i = 0; i < nbuttons; i++) {
      var v = HEAPF32[(buttons >> 2) + i];
      bs.push({ 'pressed': v >= 0.5, 'touched': v > 0, 'value': v });
    }
    for (var j = 0; j < naxes; j++) as.push(HEAPF32[(axes >> 2) + j]);
    var record = { count: 0, motors: [0, 0, 0, 0], ms: 0 };
    var actuator = null;
    if (effects) {
      var kinds = [];
      if (effects & 1) kinds.push('dual-rumble');
      if (effects & 2) kinds.push('trigger-rumble');
      actuator = {
        'effects': kinds,
        'playEffect': function (type, params) {
          record.count += 1;
          if (type === 'dual-rumble') { record.motors[0] = params.strongMagnitude; record.motors[1] = params.weakMagnitude; }
          else { record.motors[2] = params.leftTrigger; record.motors[3] = params.rightTrigger; }
          record.ms = params.duration;
          return Promise.resolve('complete');
        },
        'reset': function () {
          record.count += 1;
          record.motors = [0, 0, 0, 0];
          record.ms = -1;   // a reset, told apart from an effect of no strength
          return Promise.resolve('complete');
        },
      };
    }
    pads[slot] = {
      'index': slot, 'id': UTF8ToString(id), 'connected': true, 'mapping': standard ? 'standard' : '', 'buttons': bs,
      'axes': as, 'timestamp': performance.now(), 'vibrationActuator': actuator, windowtestsRecord: record,
    };
  },

  // WIN-26: the tests' pad at `slot` as last updated at `ms`, on `performance.now()`'s timeline: its `timestamp`, which
  // the package reads as its events' time.
  windowtests_web_pad_at: function (slot, ms) {
    var p = globalThis.windowtestsPads && globalThis.windowtestsPads[slot];
    if (p) p['timestamp'] = ms;
  },

  // How many effects the tests' pad at `slot` was asked to play, the last ones' motors (strong, weak, left, right) and
  // length, -1 after a reset; 0 when none.
  windowtests_web_rumbled: function (slot, motors, ms) {
    var p = globalThis.windowtestsPads && globalThis.windowtestsPads[slot];
    if (!p || !p.windowtestsRecord) return 0;
    var r = p.windowtestsRecord;
    for (var i = 0; i < 4; i++) HEAPF32[(motors >> 2) + i] = r.motors[i];
    HEAP32[ms >> 2] = r.ms;
    return r.count;
  },
});
