opts = { b: 2, c: 3 }
h = { a: 1, **opts }
puts h[:a]
puts h[:b]
puts h[:c]

extra = { z: 26 }
merged = { x: 24, **extra, y: 25 }
puts merged[:x]
puts merged[:y]
puts merged[:z]

def build(type, **options)
  { type: type, **options }
end
r = build("widget", color: "red", size: 10)
puts r[:type]
puts r[:color]
puts r[:size]
