re = /hello/
puts re.inspect
puts re.match("say hello there").to_s

ri = /hello/i
puts ri.match("say HELLO there").to_s

puts "one two three".sub(/\w+/, "X")
puts "one two three".gsub(/\w+/, "X")
p "one two three".scan(/\w+/)

puts 10 / 2
puts 9 / 3

puts "found" if "hello world" =~ /world/

p "2024-04-24".scan(/(\d{4})-(\d{2})-(\d{2})/)

case "hello"
when /ell/
  puts "matched ell"
else
  puts "no match"
end

p(/foo/.match("foobar").to_s)
