$stdout.sync = true

# Hash.[] — create hash from flat pairs or array-of-pairs

# Sequential flat pairs
h = Hash["a", 1, "b", 2, "c", 3]
puts h["a"]   # 1
puts h["b"]   # 2
puts h["c"]   # 3
puts h.length # 3

# Symbol keys
h2 = Hash[:x, 10, :y, 20]
puts h2[:x]   # 10
puts h2[:y]   # 20

# Single array arg: array of [key, value] pairs
h3 = Hash[ [[:p, 100], [:q, 200]] ]
puts h3[:p]   # 100
puts h3[:q]   # 200

# zip pattern
keys = [:a, :b]
vals = [1, 2]
h4 = Hash[keys.zip(vals)]
puts h4[:a]   # 1
puts h4[:b]   # 2

# Empty
puts Hash[].empty?  # true

# Odd number of flat args raises ArgumentError
begin
  Hash["a", 1, "b"]
rescue ArgumentError => e
  puts e.message
end
