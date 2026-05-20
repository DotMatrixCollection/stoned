$stdout.sync = true

arr = [1, 2]
def arr.label
  "arr-singleton"
end
arr.freeze

arr_clone = arr.clone
puts arr_clone.equal?(arr)
puts arr_clone.frozen?
puts arr_clone.label
puts arr_clone.respond_to?(:label)

arr_dup = arr.dup
puts arr_dup.frozen?
puts arr_dup.respond_to?(:label)

h = {a: 1}
def h.label
  "hash-singleton"
end
h.default_proc = proc { |hash, key| key.to_s }
h.freeze

h_clone = h.clone
puts h_clone.equal?(h)
puts h_clone.frozen?
puts h_clone.label
puts h_clone.respond_to?(:label)
puts h_clone[:missing]
puts h_clone.default_proc.nil?

h_dup = h.dup
puts h_dup.frozen?
puts h_dup.respond_to?(:label)
puts h_dup[:other]
puts h_dup.default_proc.nil?
