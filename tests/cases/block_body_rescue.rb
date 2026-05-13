result = []
loop do
  result << :body
  raise "boom"
rescue
  result << :rescued
  break
end

p result
