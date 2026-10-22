"""Capture actual X11 window pixels inside the private VM; no resizing/redraw."""
import argparse
import ctypes as C
from pathlib import Path


class XImage(C.Structure):
    _fields_=[('width',C.c_int),('height',C.c_int),('xoffset',C.c_int),('format',C.c_int),
              ('data',C.c_void_p),('byte_order',C.c_int),('bitmap_unit',C.c_int),
              ('bitmap_bit_order',C.c_int),('bitmap_pad',C.c_int),('depth',C.c_int),
              ('bytes_per_line',C.c_int),('bits_per_pixel',C.c_int),
              ('red_mask',C.c_ulong),('green_mask',C.c_ulong),('blue_mask',C.c_ulong)]


def main():
    from PIL import Image
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display',default=':98')
    parser.add_argument('--window',type=lambda value:int(value,0),required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    x=C.CDLL('libX11.so.6');pointer=C.c_void_p;number=C.c_ulong;integer=C.c_int;unsigned=C.c_uint
    x.XOpenDisplay.argtypes=[C.c_char_p];x.XOpenDisplay.restype=pointer
    x.XGetGeometry.argtypes=[pointer,number,C.POINTER(number),C.POINTER(integer),C.POINTER(integer),C.POINTER(unsigned),C.POINTER(unsigned),C.POINTER(unsigned),C.POINTER(unsigned)]
    x.XGetImage.argtypes=[pointer,number,integer,integer,unsigned,unsigned,number,integer];x.XGetImage.restype=C.POINTER(XImage)
    x.XDestroyImage.argtypes=[C.POINTER(XImage)];x.XCloseDisplay.argtypes=[pointer]
    display=x.XOpenDisplay(args.display.encode())
    if not display:raise RuntimeError('X display unavailable')
    image=None
    try:
        root=number();px=integer();py=integer();width=unsigned();height=unsigned();border=unsigned();depth=unsigned()
        if not x.XGetGeometry(display,args.window,C.byref(root),C.byref(px),C.byref(py),C.byref(width),C.byref(height),C.byref(border),C.byref(depth)):
            raise RuntimeError('Target window unavailable')
        if not 1<=width.value<=8192 or not 1<=height.value<=8192 or root.value==args.window:
            raise ValueError('A bounded non-root window is required')
        image=x.XGetImage(display,args.window,0,0,width,height,number(-1),2)
        if not image:raise RuntimeError('Window pixels unavailable')
        value=image.contents
        if (value.byte_order,value.bits_per_pixel,value.red_mask,value.green_mask,value.blue_mask)!=(0,32,0xff0000,0xff00,0xff):
            raise RuntimeError('Unsupported XImage pixel layout')
        pixels=C.string_at(value.data,value.bytes_per_line*value.height)
        Image.frombytes('RGB',(value.width,value.height),pixels,'raw','BGRX',value.bytes_per_line,1).save(args.output)
        print(f'GE6_X11_WINDOW_CAPTURE_SAVED xid={args.window:#x} width={value.width} height={value.height} path={args.output}',flush=True)
    finally:
        if image:x.XDestroyImage(image)
        x.XCloseDisplay(display)


if __name__=='__main__':
    main()
