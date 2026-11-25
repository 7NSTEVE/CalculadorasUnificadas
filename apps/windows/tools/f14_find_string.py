import glob
import os

na = b'A problem occurred while accessing the registry'
nu = na.decode().encode('utf-16-le')

roots = [
    r'C:\Windows\SysWOW64',
    r'C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series',
]
names = ['mfc100.dll', 'mfc100u.dll', 'mfc140.dll', 'mfc140u.dll',
         'MSVCR100.dll', 'MpLangRes.dll', 'Insttool.dll',
         'ActivationFxMb.dll',
         'fx-CG_Manager_PLUS_Subscription_for_fx-CG50series.exe']
for root in roots:
    for name in names:
        f = os.path.join(root, name)
        if not os.path.isfile(f):
            continue
        d = open(f, 'rb').read()
        ia = d.find(na)
        iu = d.find(nu)
        print('%-70s ascii=%s utf16=%s' % (f, hex(ia) if ia >= 0 else '-', hex(iu) if iu >= 0 else '-'))
