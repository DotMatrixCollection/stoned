STDOUT.close

begin
  puts "x"
rescue => e
  STDERR.puts e.class
  STDERR.puts e.message
end

begin
  print "y"
rescue => e
  STDERR.puts e.class
  STDERR.puts e.message
end

begin
  p 1
rescue => e
  STDERR.puts e.class
  STDERR.puts e.message
end
