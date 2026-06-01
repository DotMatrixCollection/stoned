e = [10, 20].each
p e.peek
p e.next
p e.peek
p e.next
begin
  e.next
rescue StopIteration => ex
  puts ex.class
end
