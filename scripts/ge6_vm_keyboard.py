"""Deliver a bounded, real XTest key press to the focused diagnostic window.

Run inside the private Linux VM. This helper does not call game functions or
change game memory. A key is always released, including on interruption.
"""
import argparse
import ctypes as C
import json
import os
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--window', type=lambda value: int(value, 0), required=True)
    parser.add_argument('--key', required=True, help='X keysym name, e.g. Return or w')
    parser.add_argument('--hold', type=float, default=0.15)
    args = parser.parse_args()
    if not 0 < args.hold <= 30:
        parser.error('--hold must be between 0 and 30 seconds')
    x = C.CDLL('libX11.so.6')
    xt = C.CDLL('libXtst.so.6')
    x.XOpenDisplay.argtypes = [C.c_char_p]
    x.XOpenDisplay.restype = C.c_void_p
    x.XGetInputFocus.argtypes = [C.c_void_p, C.POINTER(C.c_ulong), C.POINTER(C.c_int)]
    x.XGetInputFocus.restype = C.c_int
    x.XStringToKeysym.argtypes = [C.c_char_p]
    x.XStringToKeysym.restype = C.c_ulong
    x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
    x.XKeysymToKeycode.restype = C.c_ubyte
    x.XSync.argtypes = [C.c_void_p, C.c_int]
    x.XCloseDisplay.argtypes = [C.c_void_p]
    xt.XTestFakeKeyEvent.argtypes = [C.c_void_p, C.c_uint, C.c_int, C.c_ulong]
    xt.XTestFakeKeyEvent.restype = C.c_int
    d = x.XOpenDisplay(os.environ.get('DISPLAY', ':98').encode())
    if not d:
        raise SystemExit('X display unavailable')
    pressed = False
    try:
        focus, revert = C.c_ulong(), C.c_int()
        x.XGetInputFocus(d, C.byref(focus), C.byref(revert))
        if focus.value != args.window:
            raise SystemExit('Target window does not own actual keyboard focus')
        code = x.XKeysymToKeycode(d, x.XStringToKeysym(args.key.encode('ascii')))
        if not code:
            raise SystemExit('Key is absent from the actual server layout')
        if not xt.XTestFakeKeyEvent(d, code, 1, 0):
            raise SystemExit('XTest key press failed')
        pressed = True
        x.XSync(d, 0)
        print(json.dumps(dict(event='key-down', window=args.window, key=args.key,
                              keycode=code, monotonic=time.monotonic())), flush=True)
        time.sleep(args.hold)
    finally:
        if pressed:
            result = xt.XTestFakeKeyEvent(d, code, 0, 0)
            x.XSync(d, 0)
            print(json.dumps(dict(event='key-up', window=args.window, key=args.key,
                                  keycode=code, result=result,
                                  monotonic=time.monotonic())), flush=True)
        x.XCloseDisplay(d)


if __name__ == '__main__':
    main()
