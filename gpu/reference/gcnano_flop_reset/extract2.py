import re
src=open('gcnano/gcnano-driver-stm32mp-6.4.15-stm32mp2-r1-rc3/hal/kernel/arch/gc_hal_kernel_hardware_func_flop_reset.c').read()
lines=src.split('\n'); body=[]; inbody=False
for l in lines:
    if 'commands = (gctUINT32_PTR)bufferLogical;' in l: inbody=True; continue
    if inbody and 'bytes = gcmSIZEOF(gctUINT32) * index;' in l: break
    if inbody: body.append(l)
text='\n'.join(body)
def resolve_halti5(t):
    idx=t.find('gcvFEATURE_HALTI5')
    if idx<0: return t
    ifpos=t.rfind('if',0,idx)
    b=t.find('{',idx); depth=0; j=b
    while j<len(t):
        depth+= (t[j]=='{')-(t[j]=='}')
        if depth==0: break
        j+=1
    A=t[b+1:j]; rest=t[j+1:]; eb=rest.find('{',rest.find('else')); d=0; k=eb
    while k<len(rest):
        d+=(rest[k]=='{')-(rest[k]=='}')
        if d==0: break
        k+=1
    return t[:ifpos]+A+rest[k+1:]
text=resolve_halti5(text)
stmts=re.findall(r'commands\[index\+\+\]\s*=\s*(.*?);', text, re.S)
VARS=['inImageAddress','outImageAddress','instAddress','Stride','Width','Height','RegCount','InstCount','ThreadAllocation','GlobalOffsetX','GlobalOffsetY','GlobalOffsetZ','GlobalScaleX','GlobalScaleY','GlobalScaleZ','groupCountX','groupCountY','groupCountZ','GroupSizeX','GroupSizeY','GroupSizeZ','ValueOrder','WorkDim']

def conv_tern(x):
    # convert innermost C ternary a?b:c -> (b if(a)else c); repeat
    while '?' in x:
        q=x.rfind('?')  # rightmost, then its matching : is next ':' at depth 0
        # condition: scan left from q for balanced '(' start
        d=0; i=q-1
        while i>=0:
            if x[i]==')':d+=1
            elif x[i]=='(':
                if d==0: break
                d-=1
            i+=-1
        cs=i+1
        # ':' matching q at depth 0
        d=0; j=q+1
        while j<len(x):
            if x[j]=='(':d+=1
            elif x[j]==')':
                if d==0: break
                d-=1
            elif x[j]==':' and d==0: break
            j+=1
        colon=j
        # end: scan right from colon for balanced ')'
        d=0; k=colon+1
        while k<len(x):
            if x[k]=='(':d+=1
            elif x[k]==')':
                if d==0: break
                d-=1
            k+=1
        ce=k
        cond=x[cs:q]; a=x[q+1:colon]; b=x[colon+1:ce]
        x=x[:cs]+'('+a+' if('+cond+')else '+b+')'+x[ce:]
    return x

def ev(e):
    x=re.sub(r'\s+',' ',e).strip()
    if any(re.search(r'\b'+v+r'\b',x) for v in VARS): return None
    x=x.replace('(gctUINT32)','').replace('~0U','0xFFFFFFFF')
    x=conv_tern(x)
    return eval(x,{'__builtins__':{}},{}) & 0xFFFFFFFF

out=[]
for s in stmts:
    r=ev(s)
    out.append(('VAR',re.sub(r'\s+',' ',s).strip()) if r is None else ('LIT',r))
print('total:',len(out),'  first 12:')
for i,(k,v) in enumerate(out[:12]):
    print(f'{i:3} {k:3} '+('0x%08X'%v if k=='LIT' else v))
open('ppu_cmd_seq.txt','w').write('\n'.join((f'LIT 0x{v:08X}' if k=='LIT' else 'VAR '+v) for k,v in out))
print('wrote ppu_cmd_seq.txt (',len(out),'entries)')
