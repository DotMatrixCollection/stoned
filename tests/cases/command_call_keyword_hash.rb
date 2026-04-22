def capture(*args)
  puts args.inspect
end

class Box
  def capture(*args)
    puts args.inspect
  end
end

capture a: 1
capture(a: 1)
capture 1, a: 1, b: 2
capture(1, a: 1, b: 2)

box = Box.new
box.capture a: 1
box.capture(a: 1)
box.capture 1, a: 1, b: 2
box.capture(1, a: 1, b: 2)
