begin
  File.read("/tmp/stoned_missing_hierarchy_test.txt")
rescue SystemCallError => e
  puts e.class
  puts e.is_a?(Errno::ENOENT)
  puts e.is_a?(SystemCallError)
  puts e.is_a?(StandardError)
  puts e.is_a?(Exception)
end
