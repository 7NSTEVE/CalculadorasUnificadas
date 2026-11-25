import struct

path = r'C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe'
data = open(path, 'rb').read()
# .data: vaddr 0x67000, rptr 0x65200, rsize 0x2C00
def rva2off(rva):
    return 0x65200 + (rva - 0x67000)

start = 0x67400
end = 0x67700
for rva in range(start, end, 4):
    off = rva2off(rva)
    val = struct.unpack_from('<I', data, off)[0]
    text = ''
    # marcar valores que parecen VA de .text/.rdata
    if 0x401000 <= val < 0x44F549:
        text = ' <.text VA>'
    elif 0x452000 <= val < 0x466800:
        text = ' <.rdata VA>'
    elif 0x466800 <= val < 0x46B824:
        text = ' <.data VA>'
    elif val < 0x400:
        text = ' (small)'
    print(f'RVA 0x{rva:05X}: 0x{val:08X}{text}')
