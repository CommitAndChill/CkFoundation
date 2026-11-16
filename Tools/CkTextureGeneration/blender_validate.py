"""Lossless native PNG decoding extracted from the accepted validation path."""
from __future__ import annotations
from pathlib import Path
import struct
import zlib
import numpy as np

def decode_native_png(path:Path|bytes)->np.ndarray:
    """Decode noninterlaced grayscale/RGB/GA/RGBA PNG8/16 losslessly.

    Returns HxWxC uint8/uint16 sample values, NOT normalized or color-managed.
    PNG filters are reversed on bytes before big-endian16 samples are unpacked.
    Ancillary metadata cannot affect decoded samples. Palette/interlace rejected.
    """
    blob=path if isinstance(path,bytes) else Path(path).read_bytes()
    if blob[:8]!=b'\x89PNG\r\n\x1a\n':raise ValueError('Not PNG')
    offset=8;parts=[];header=None
    while offset<len(blob):
        if offset+12>len(blob):raise ValueError('Truncated PNG chunk')
        length=struct.unpack('>I',blob[offset:offset+4])[0];kind=blob[offset+4:offset+8]
        end=offset+8+length
        if end+4>len(blob):raise ValueError('Truncated PNG payload')
        data=blob[offset+8:end]
        if zlib.crc32(kind+data)&0xffffffff!=struct.unpack('>I',blob[end:end+4])[0]:raise ValueError('PNG CRC mismatch')
        if kind==b'IHDR':header=struct.unpack('>IIBBBBB',data)
        elif kind==b'IDAT':parts.append(data)
        elif kind==b'IEND':break
        offset=end+4
    if header is None:raise ValueError('Missing IHDR')
    w,h,depth,color,compression,filtering,interlace=header
    channels={0:1,2:3,4:2,6:4}.get(color)
    if channels is None or depth not in (8,16) or compression or filtering or interlace:
        raise ValueError(f'Unsupported native PNG format: {header}')
    bpp=channels*(depth//8);stride=w*bpp
    filtered=zlib.decompress(b''.join(parts))
    if len(filtered)!=h*(stride+1):raise ValueError('Incorrect decompressed PNG size')
    output=np.empty((h,stride),dtype=np.uint8);previous=np.zeros(stride,dtype=np.uint8)
    for y in range(h):
        start=y*(stride+1);method=filtered[start]
        row=np.frombuffer(filtered,dtype=np.uint8,count=stride,offset=start+1).copy()
        if method==1:
            # Each byte lane is an independent cumulative sum modulo256.
            row=np.cumsum(row.reshape(w,bpp),axis=0,dtype=np.uint32).astype(np.uint8).reshape(-1)
        elif method==2:row=np.add(row,previous,dtype=np.uint8)
        elif method in (3,4):
            values=row.tolist();up=previous.tolist()
            for i in range(stride):
                left=values[i-bpp] if i>=bpp else 0;above=up[i];upperleft=up[i-bpp] if i>=bpp else 0
                if method==3:predictor=(left+above)//2
                else:
                    p=left+above-upperleft;pa=abs(p-left);pb=abs(p-above);pc=abs(p-upperleft)
                    predictor=left if pa<=pb and pa<=pc else above if pb<=pc else upperleft
                values[i]=(values[i]+predictor)&255
            row=np.asarray(values,dtype=np.uint8)
        elif method!=0:raise ValueError(f'Unsupported PNG filter {method}')
        output[y]=row;previous=row
    if depth==16:return np.frombuffer(output.tobytes(),dtype='>u2').astype(np.uint16).reshape(h,w,channels)
    return output.reshape(h,w,channels)
