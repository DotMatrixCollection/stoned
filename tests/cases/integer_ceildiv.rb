puts 7.ceildiv(3)     # 3
puts 8.ceildiv(4)     # 2
puts 10.ceildiv(3)    # 4
puts(-7.ceildiv(3))   # -2
puts 7.ceildiv(-3)    # -2
begin
  7.ceildiv(0)
rescue ZeroDivisionError => e
  puts e.message
end
