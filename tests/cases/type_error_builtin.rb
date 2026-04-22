begin
  {a: 1}.merge([])
rescue TypeError => e
  puts e.class
  puts e.message
end
