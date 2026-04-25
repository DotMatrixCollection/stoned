re = Regexp.new("ell")
puts re.source
puts re.inspect
puts "hello".match(re).to_s
puts "hello".match(re)[0]
puts "hello".match(re).begin(0)
puts "hello".match(re).end(0)
puts "hello".match(re).pre_match
puts "hello".match(re).post_match
puts re.match("hello").to_s
puts("hello" =~ re)
puts(re =~ "hello")
p "zzz".match(re)

begin
  Regexp.new("e.l")
rescue RegexpError => e
  puts e.message
end
