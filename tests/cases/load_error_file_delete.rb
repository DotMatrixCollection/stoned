begin
  File.delete("/tmp/stoned_io_missing_delete.txt")
rescue LoadError => e
  puts e.class
  puts e.message
end
