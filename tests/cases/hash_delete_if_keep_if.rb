h = {a: 1, b: 2, c: 3, d: 4}
h.delete_if { |k, v| v > 2 }
puts h.inspect

h2 = {a: 1, b: 2, c: 3, d: 4}
h2.keep_if { |k, v| v.odd? }
puts h2.inspect

# reject! returns nil when nothing deleted
h3 = {a: 1, b: 2}
result = h3.reject! { |k, v| v > 100 }
puts result.inspect
puts h3.inspect

# select! returns nil when nothing removed
h4 = {x: 5, y: 10}
result2 = h4.select! { |k, v| v > 0 }
puts result2.inspect
puts h4.inspect
