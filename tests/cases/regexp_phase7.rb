m = /(\w+)\s+(\w+)/.match("hello world")
puts m[0]
puts m[1]
puts m[2]
puts m[-1]
puts m.captures.inspect
puts m.begin(0)
puts m.begin(1)
puts m.end(2)
puts m.length

puts /ell/.match?("hello")
puts /xyz/.match?("hello")
puts "hello".match?(/ell/)
puts "hello".match?("ell")

puts /foo/.options
puts /foo/i.options
puts /foo/im.options

puts Regexp.escape("hello.world+foo?")
puts Regexp.quote("1+1=2")

/(\w+)/.match("hello") { |md| puts md[1] }
"hello world".match(/(\w+)\s+(\w+)/) { |md| puts md.captures.inspect }

p "one  two   three".split(/\s+/)
p "a1b2c3".split(/(\d)/)
p "hello".split(/x/)
p "a,b,,c,".split(/,/)
p "a,b,,c,".split(/,/, -1)
p "a,b,c".split(/,/, 2)
