def capture(*args)
  puts args.inspect
end

class Box
  def capture(*args)
    puts args.inspect
  end
end

capture 1,
  2
capture(1,
  2)

box = Box.new
box.capture 1,
  2
box.capture(1,
  2)
