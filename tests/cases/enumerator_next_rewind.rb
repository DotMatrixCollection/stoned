enum = [1, 2, 3].each
p enum.next
p enum.next
p enum.next
begin
  enum.next
rescue StopIteration => e
  p "StopIteration caught"
end

enum.rewind
p enum.next
