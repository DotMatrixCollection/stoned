class Box
end

box = Box.new

result = class << box
  42
end

puts result

class << box
  def hi
    7
  end
end

puts box.hi
