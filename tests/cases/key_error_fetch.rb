begin
  {}.fetch(:missing)
rescue KeyError => e
  puts e.class
  puts e.message
end
