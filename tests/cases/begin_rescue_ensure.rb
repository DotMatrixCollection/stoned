begin
  raise "boom"
rescue
  puts "rescued"
ensure
  puts "ensure"
end

puts "after"
