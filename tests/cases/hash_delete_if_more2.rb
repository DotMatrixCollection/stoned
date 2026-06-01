h = {a: 1, b: 2, c: 3}
p h.delete_if { |k, v| v < 3 }
p h.keys
