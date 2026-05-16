puts Kernel.class

class DelayProbe
  def initialize(delay)
    @delay_until = (Time.now + delay if delay)
  end

  def delay_until
    @delay_until
  end
end

puts DelayProbe.new(nil).delay_until.nil?
