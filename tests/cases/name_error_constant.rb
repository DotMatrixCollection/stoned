begin
  MissingConstant
rescue NameError => e
  puts e.class
  puts e.message
end
