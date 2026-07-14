resolve={
 'Stride':0x40,'Height << 16 | Width':0x00060040,'RegCount':3,
 'InstCount / 4':4,'InstCount / 4 - 1':3,'ThreadAllocation':1,
 'GlobalOffsetX':0,'GlobalOffsetY':0,'GlobalOffsetZ':0,
 'GlobalScaleX':4,'GlobalScaleY':1,'GlobalScaleZ':0,
 'groupCountX - 1':15,'groupCountY - 1':5,'groupCountZ - 1':0xFFFFFFFF,
}
addr={'inImageAddress':'IN','outImageAddress':'OUT','instAddress':'INST'}
lines=[l.rstrip() for l in open('ppu_cmd_seq.txt') if l.strip()]
vals=[]; patch={}
for i,l in enumerate(lines):
    kind,rest=l.split(' ',1)
    if kind=='LIT':
        vals.append(int(rest,16))
    else:
        r=rest.strip()
        if r in addr:
            patch[i]=addr[r]; vals.append(0)  # runtime address
        elif r in resolve:
            vals.append(resolve[r]&0xFFFFFFFF)
        elif 'GroupSizeX' in r: vals.append(0)       # GroupSizeX-1 = 0, [9:0]
        elif 'GroupSizeY' in r: vals.append(0)       # GroupSizeY-1 = 0
        elif 'GroupSizeZ' in r: vals.append((0-1)&0x3FF) # 0x3FF
        else:
            raise SystemExit('unresolved: '+r)
print('// %d dwords; address patch slots: %s'%(len(vals), patch))
# emit C++ array, 8 per line
s='static const uint32_t kPpuDispatch[%d] = {\n'%len(vals)
for i in range(0,len(vals),8):
    s+='\t'+' '.join('0x%08X,'%v for v in vals[i:i+8])+'\n'
s+='};\n'
# patch indices
s+='// runtime address patch indices (dword offsets):\n'
for i,name in patch.items(): s+='//   [%d] = %s\n'%(i,name)
open('kppu.txt','w').write(s)
print(s)
