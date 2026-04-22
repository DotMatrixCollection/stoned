begin
  "abc".public_send(1)
rescue TypeError => e
  puts e.class
  puts e.message
end
