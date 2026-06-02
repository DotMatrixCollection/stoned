class CallbackBox
  def initialize(&block)
    @block = block
  end

  def call(value)
    @block.call(value)
  end
end

factor = 3
box = CallbackBox.new { |n| n * factor }

p box.call(4)
factor = 5
p box.call(4)

other = CallbackBox.new do |text|
  text.upcase
end

p other.call("ruby")
