class Counter
  @@count = 0
  def initialize = (@@count += 1)
  def self.count = @@count
  def self.reset = (@@count = 0)
end
Counter.new; Counter.new; Counter.new
p Counter.count
Counter.reset
p Counter.count
Counter.new
p Counter.count
