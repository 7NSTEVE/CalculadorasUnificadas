import frida

session = frida.attach(32388)
script = session.create_script('''
Process.enumerateModules().forEach(function(m) {
    send({name: m.name, base: m.base.toString(), path: m.path});
});
''')

def on_message(msg, data):
    if msg['type'] == 'send':
        p = msg['payload']
        print(f"{p['name']} at {p['base']}: {p['path']}")

script.on('message', on_message)
script.load()
session.detach()
