double = ->(n) { n * 2 }
puts double.(3)

ping = -> { 7 }
puts ping.()

class Box
  def initialize(v)
    @v = v
  end

  def call(x)
    @v + x
  end
end

puts Box.new(4).(5)
