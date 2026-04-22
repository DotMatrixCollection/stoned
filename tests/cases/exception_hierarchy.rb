begin
  nil.missing
rescue NameError => e
  puts e.class
end

begin
  raise StopIteration.new("done")
rescue StandardError => e
  puts e.class
  puts e.message
end

begin
  File.read(1)
rescue StandardError => e
  puts e.class
end
