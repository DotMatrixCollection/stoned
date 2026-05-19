t = Time.now
r = /x/

puts t.respond_to?(:year)
puts r.respond_to?(:source)
puts r.respond_to?(:options)
