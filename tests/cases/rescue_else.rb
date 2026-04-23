begin
  puts "body-ok"
rescue
  puts "rescued"
else
  puts "else-ok"
ensure
  puts "ensure-ok"
end

begin
  raise "boom"
rescue RuntimeError
  puts "rescued-boom"
else
  puts "else-boom"
ensure
  puts "ensure-boom"
end

def classify(x)
  raise ArgumentError, "neg" if x < 0
  x * 2
rescue ArgumentError
  :neg
else
  x + 100
ensure
  puts "ensure-#{x}"
end

puts classify(5)
puts classify(-1)
