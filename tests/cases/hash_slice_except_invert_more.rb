settings = {host: "localhost", port: 9292, debug: false, token: "secret"}

p settings.slice(:host, :port, :missing)
p settings.except(:token, :debug)
p({a: 1, b: 2, c: 1}.invert)
p settings.key(9292)
p settings.index(false)
