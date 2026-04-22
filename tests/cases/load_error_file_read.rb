begin
  File.read("/tmp/stoned_io_missing.txt")
rescue LoadError => e
  puts e.class
  puts e.message
end
