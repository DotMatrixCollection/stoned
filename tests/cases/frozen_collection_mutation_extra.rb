$stdout.sync = true

a = [1, 2].freeze

begin
  a.clear
rescue FrozenError => e
  puts "array clear: #{e.class}"
end

begin
  a.shift
rescue FrozenError => e
  puts "array shift: #{e.class}"
end

begin
  a.unshift(0)
rescue FrozenError => e
  puts "array unshift: #{e.class}"
end

begin
  a[0] = 9
rescue FrozenError => e
  puts "array []=: #{e.class}"
end

begin
  a.compact!
rescue FrozenError => e
  puts "array compact!: #{e.class}"
end

h = {a: 1}.freeze

begin
  h.delete(:a)
rescue FrozenError => e
  puts "hash delete: #{e.class}"
end

begin
  h.clear
rescue FrozenError => e
  puts "hash clear: #{e.class}"
end

begin
  h.merge!(b: 2)
rescue FrozenError => e
  puts "hash merge!: #{e.class}"
end

begin
  h.transform_values! { |v| v + 1 }
rescue FrozenError => e
  puts "hash transform_values!: #{e.class}"
end

begin
  h.transform_keys! { |k| :"x_#{k}" }
rescue FrozenError => e
  puts "hash transform_keys!: #{e.class}"
end

begin
  h.default = 3
rescue FrozenError => e
  puts "hash default=: #{e.class}"
end

begin
  h.default_proc = proc { |hash, key| key }
rescue FrozenError => e
  puts "hash default_proc=: #{e.class}"
end
