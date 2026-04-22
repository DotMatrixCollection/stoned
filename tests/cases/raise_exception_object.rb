e = RuntimeError.new

begin
  raise e
rescue RuntimeError => caught
  puts caught.equal?(e)
  puts caught.message
end
