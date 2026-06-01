a = "key"
b = "key"

puts a == b
puts a.equal?(b)

h = {}
puts h.compare_by_identity?
puts h.respond_to?(:compare_by_identity)
puts h.methods.include?(:compare_by_identity?)
puts h.compare_by_identity.equal?(h)
puts h.compare_by_identity?

h[a] = 1
h[b] = 2

puts h.length
puts h[a]
puts h[b]
puts h.fetch("key", "missing")
puts h.key?(a)
puts h.key?("key")

copy = h.dup
puts copy.compare_by_identity?
puts copy[a]
puts copy[b]
puts copy.fetch("key", "missing")

plain = {}
plain[a] = 1
plain[b] = 2
puts plain.length
puts plain[a]
