h = Hash.new(0)
h[:a] += 1
h[:a] += 1
h[:b] += 5
p h[:a]
p h[:b]
p h[:c]
p h.default
p h.default_proc

h2 = Hash.new { |hsh, k| hsh[k] = k.to_s * 2 }
p h2[:foo]
p h2[:bar]
p h2.keys.sort.map(&:to_s)
p h2.default_proc.class
