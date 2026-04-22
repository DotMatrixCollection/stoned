module Named
  def name
    "named"
  end
end

module Loud
  include Named

  def shout
    name + "!"
  end
end

class Thing
  include Loud
end

t = Thing.new
puts t.name
puts t.shout

module First
  def marker
    "first"
  end
end

module Second
  def marker
    "second"
  end
end

class Picked
  include First
  include Second
end

puts Picked.new.marker
