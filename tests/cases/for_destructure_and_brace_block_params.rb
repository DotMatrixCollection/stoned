pairs = {a: 1, b: 2}.to_a

for key, value in pairs
  puts "#{key}=#{value}"
end

mapped = pairs.collect{
  |key, value|
  "#{value}:#{key}"
}

puts mapped.join(",")
