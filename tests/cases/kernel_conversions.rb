puts Integer(3.9)
puts Integer("42")
puts Float(5)
puts Float("3.25")
puts String(:cat)
puts Array(nil).inspect
puts Array(7).inspect
puts Array([1, 2]).inspect
puts Array({a: 1}).inspect
puts Array(1..3).inspect

begin
  Integer([1, 2])
rescue TypeError => e
  puts e.class
end
