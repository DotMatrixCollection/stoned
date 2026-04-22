attempts = 0

begin
  attempts += 1
  puts attempts
  raise RuntimeError, "again" if attempts < 2
rescue RuntimeError
  retry
end

puts "done"
