t = Time.now
r = /x/

puts t.respond_to?(:year)
puts t.methods.include?(:year)
puts t.methods.include?(:strftime)
puts t.methods.include?(:to_i)
puts r.respond_to?(:source)
puts r.methods.include?(:source)
puts r.respond_to?(:options)
puts r.methods.include?(:options)
