begin
  File.read("/tmp/stoned_io_missing.txt")
rescue Errno::ENOENT => e
  puts e.class
  puts e.message
end
