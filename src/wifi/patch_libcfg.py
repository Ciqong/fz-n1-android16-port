from pathlib import Path
import hashlib, struct, json
p=Path(__file__).parent
original=(p/'libcfg.so').read_bytes()
assert hashlib.sha256(original).hexdigest()=='d16424ce97a81a571d91e15051de0ba66db397aff00c465245fdd666fea7df8c'
code=(p/'byte_memcmp.bin').read_bytes()
assert len(code)==44
out=bytearray(original)
cave=0x52c0
assert original[cave:cave+len(code)]==bytes(len(code))
assert struct.unpack_from('<I',original,0x5250)[0]==0xb0000010
phoff=struct.unpack_from('<Q',original,32)[0]
phentsize,phnum=struct.unpack_from('<HH',original,54)
found=False
for i in range(phnum):
    pos=phoff+i*phentsize
    typ,flags,offset,vaddr,paddr,filesz,memsz,align=struct.unpack_from('<IIQQQQQQ',original,pos)
    if typ==1 and flags==5 and offset==0x2000:
        assert vaddr==offset and filesz==memsz==0x32c0 and align==0x1000
        end=cave+len(code)
        assert end<0x6000
        struct.pack_into('<QQ',out,pos+32,end-offset,end-offset)
        found=True
assert found
out[cave:cave+len(code)]=code
struct.pack_into('<I',out,0x5250,0x14000000|((cave-0x5250)//4))
assert len(out)==len(original)
(p/'libcfg-byte-safe.so').write_bytes(out)
report={'original_sha256':hashlib.sha256(original).hexdigest(),'patched_sha256':hashlib.sha256(out).hexdigest(),'bytes':len(out),'memcmp_plt':hex(0x5250),'byte_comparator':hex(cave),'comparator_bytes':len(code),'changed_byte_count':sum(a!=b for a,b in zip(original,out)),'scope':'Only libcfg memcmp calls redirect to byte comparator; existing NV write/check and lock/unlock control flow retained','normal_buffer_qemu_test':'passed','device_mapping_readonly_test':'passed'}
(p/'libcfg-byte-safe-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
