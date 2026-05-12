begin
  IO.new(9999, "r")
rescue Errno::EBADF => e
  puts e.class
  puts e.message
end
