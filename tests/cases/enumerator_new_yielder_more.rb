e = Enumerator.new do |y|
  y << 1
  y << 2
  y << 3
end
p e.next
p e.next
p e.to_a
