begin
  $stdout = 123
rescue => e
  STDOUT.puts e.class
  STDOUT.puts e.message
end

begin
  $stderr = 123
rescue => e
  STDOUT.puts e.class
  STDOUT.puts e.message
end
