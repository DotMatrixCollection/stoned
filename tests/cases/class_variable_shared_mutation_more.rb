class SharedCounterBase
  @@count = 0

  def self.bump
    @@count += 1
  end

  def self.count
    @@count
  end
end

class SharedCounterChild < SharedCounterBase
end

p SharedCounterBase.bump
p SharedCounterChild.bump
p SharedCounterBase.count
p SharedCounterChild.count
