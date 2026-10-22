"""Exercise the original GE6 UI through actual XTest input in the private Linux VM.

Run inside the guest. Supply a window ID observed in xwininfo/GE6 logs.
This sends pointer input only; it does not call game functions or edit scripts.
"""
import argparse
import ctypes as C
import time


def click(display_name,window,x,y,repeat,interval,hold):
    x11=C.CDLL('libX11.so.6');xtst=C.CDLL('libXtst.so.6')
    pointer=C.c_void_p;number=C.c_ulong;integer=C.c_int;unsigned=C.c_uint
    x11.XOpenDisplay.argtypes=[C.c_char_p];x11.XOpenDisplay.restype=pointer
    x11.XCloseDisplay.argtypes=[pointer];x11.XSync.argtypes=[pointer,integer]
    x11.XDefaultScreen.argtypes=[pointer];x11.XDefaultScreen.restype=integer
    x11.XGetGeometry.argtypes=[pointer,number,C.POINTER(number),C.POINTER(integer),C.POINTER(integer),C.POINTER(unsigned),C.POINTER(unsigned),C.POINTER(unsigned),C.POINTER(unsigned)]
    x11.XTranslateCoordinates.argtypes=[pointer,number,number,integer,integer,C.POINTER(integer),C.POINTER(integer),C.POINTER(number)]
    x11.XQueryPointer.argtypes=[pointer,number,C.POINTER(number),C.POINTER(number),C.POINTER(integer),C.POINTER(integer),C.POINTER(integer),C.POINTER(integer),C.POINTER(unsigned)]
    xtst.XTestQueryExtension.argtypes=[pointer]+[C.POINTER(integer)]*4
    xtst.XTestFakeMotionEvent.argtypes=[pointer,integer,integer,integer,number]
    xtst.XTestFakeButtonEvent.argtypes=[pointer,unsigned,integer,number]
    display=x11.XOpenDisplay(display_name.encode())
    if not display:
        raise RuntimeError('X display unavailable')
    try:
        extension=[integer() for _ in range(4)]
        if not xtst.XTestQueryExtension(display,*(C.byref(item) for item in extension)):
            raise RuntimeError('XTest input extension unavailable')
        root=number();wx=integer();wy=integer();width=unsigned();height=unsigned();border=unsigned();depth=unsigned()
        if not x11.XGetGeometry(display,window,C.byref(root),C.byref(wx),C.byref(wy),C.byref(width),C.byref(height),C.byref(border),C.byref(depth)) or window==root.value:
            raise RuntimeError('A valid non-root target window is required')
        if not 0<=x<width.value or not 0<=y<height.value:
            raise ValueError('Click is outside the target client area')
        root_x=integer();root_y=integer();child=number()
        if not x11.XTranslateCoordinates(display,window,root,x,y,C.byref(root_x),C.byref(root_y),C.byref(child)):
            raise RuntimeError('Cannot translate client coordinates')
        for index in range(repeat):
            if not xtst.XTestFakeMotionEvent(display,x11.XDefaultScreen(display),root_x,root_y,0):
                raise RuntimeError('XTest motion failed')
            x11.XSync(display,0)
            # Follow the actual pointer hierarchy through any window-manager frame.
            target=root.value
            for _ in range(128):
                owner=number();below=number();rx=integer();ry=integer();cx=integer();cy=integer();state=unsigned()
                if not x11.XQueryPointer(display,target,C.byref(owner),C.byref(below),C.byref(rx),C.byref(ry),C.byref(cx),C.byref(cy),C.byref(state)):
                    raise RuntimeError('Pointer query failed')
                if target==window:
                    break
                target=below.value
                if not target:
                    raise RuntimeError('Target is hidden or obscured at the requested click')
            else:
                raise RuntimeError('Pointer hierarchy exceeds supported depth')
            pressed=False
            try:
                if not xtst.XTestFakeButtonEvent(display,1,1,0):
                    raise RuntimeError('XTest button press failed')
                pressed=True;x11.XSync(display,0);time.sleep(hold)
            finally:
                if pressed:
                    xtst.XTestFakeButtonEvent(display,1,0,0);x11.XSync(display,0)
            print(f'GE6_VM_XTEST_CLICK window=0x{window:x} client_x={x} client_y={y} index={index+1}',flush=True)
            if index+1<repeat:
                time.sleep(interval)
    finally:
        x11.XCloseDisplay(display)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display',default=':98')
    parser.add_argument('--window',required=True,type=lambda value:int(value,0))
    parser.add_argument('--x',required=True,type=int);parser.add_argument('--y',required=True,type=int)
    parser.add_argument('--repeat',type=int,default=1);parser.add_argument('--interval',type=float,default=4)
    parser.add_argument('--hold',type=float,default=0.2)
    args=parser.parse_args()
    if not 1<=args.repeat<=32 or not 0<=args.interval<=30 or not 0.01<=args.hold<=5:
        parser.error('Repeat, interval or hold duration is outside the test bounds')
    click(args.display,args.window,args.x,args.y,args.repeat,args.interval,args.hold)
