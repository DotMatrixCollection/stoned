class Counter
  @@count = 0
  def self.inc; @@count += 1; end
  def self.count; @@count; end
end
Counter.inc
Counter.inc
puts Counter.count
