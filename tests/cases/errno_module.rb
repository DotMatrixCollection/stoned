puts Errno::ENOENT
puts Errno::ENOENT.class
begin
  File.read("/tmp/stoned_missing_module_test.txt")
rescue => e
  puts Errno::ENOENT === e
  puts SystemCallError === e
  puts StandardError === e
end
