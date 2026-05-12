begin
  File.open("/tmp/stoned_missing_open_test.txt", "r")
rescue Errno::ENOENT => e
  puts e.class
  puts e.message
end
