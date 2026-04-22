begin
  1 / 0
rescue ZeroDivisionError => e
  puts e.class
  puts e.message
end
