puts (1..5).first(10).inspect
puts (1..5).last(10).inspect
puts (1...5).first(10).inspect
puts (1...5).last(10).inspect
puts (10..1).first(2).inspect
puts (10..1).last(2).inspect
puts (1..5).first(1.9).inspect
puts (1..5).last(1.9).inspect
begin
  (1..5).first(-1)
rescue ArgumentError => e
  puts e.class
end
begin
  (1..5).last(-1)
rescue ArgumentError => e
  puts e.class
end
begin
  (1..5).first("2")
rescue TypeError => e
  puts e.class
end
