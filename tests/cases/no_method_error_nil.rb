begin
  nil.missing
rescue NoMethodError => e
  puts e.class
  puts e.message
end
