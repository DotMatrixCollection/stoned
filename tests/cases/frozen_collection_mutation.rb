$stdout.sync = true

# Frozen Array mutation raises FrozenError

a = [1, 2, 3]
a.freeze
puts a.frozen?  # true

begin
  a << 4
rescue FrozenError => e
  puts "array <<: #{e.class}"
end

begin
  a.push(5)
rescue FrozenError => e
  puts "array push: #{e.class}"
end

begin
  a.concat([6])
rescue FrozenError => e
  puts "array concat: #{e.class}"
end

puts a.inspect  # [1, 2, 3] — unchanged

# Frozen Hash mutation raises FrozenError
h = {a: 1}
h.freeze
puts h.frozen?  # true

begin
  h[:b] = 2
rescue FrozenError => e
  puts "hash []=: #{e.class}"
end

puts h.inspect  # {a: 1} — unchanged

# Frozen String mutation raises FrozenError (already worked, confirm)
s = "hello".freeze
begin
  s << " world"
rescue FrozenError => e
  puts "string <<: #{e.class}"
end

puts s  # hello — unchanged
