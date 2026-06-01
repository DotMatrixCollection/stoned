$stdout.sync = true

a = [1, 2]
b = [3, 4, 5]
puts a.replace(b).equal?(a)
puts a.inspect
puts b.inspect

h = {a: 1}
h.default = 7
other = {b: 2}
other.default_proc = proc { |hash, key| key.to_s }
puts other.respond_to?(:default)
puts other.respond_to?(:default=)
puts other.respond_to?(:default_proc)
puts other.respond_to?(:default_proc=)
puts Hash.instance_methods.include?(:default_proc=)
puts h.replace(other).equal?(h)
puts h.inspect
puts h[:missing]
puts h.default.inspect
puts h.default_proc.nil?

begin
  [1].freeze.replace([2])
rescue => e
  puts e.class
end

begin
  ({a: 1}.freeze).replace({b: 2})
rescue => e
  puts e.class
end
