h = {a: 1}
p h.rehash.equal?(h)
p h
p h.respond_to?(:rehash)
p Hash.instance_methods.include?(:rehash)

begin
  h.freeze.rehash
rescue => e
  puts e.class
end
