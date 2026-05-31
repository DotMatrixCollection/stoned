class Base
  def self.build(x, y)
    "Base(#{x},#{y})"
  end

  def self.wrap(label)
    "Base[#{label}]"
  end
end

class Child < Base
  def self.build(x, y)
    "Child+#{super}"
  end

  def self.wrap(label)
    "Child+#{super}"
  end
end

class GrandChild < Child
  def self.build(x, y)
    "Grand+#{super}"
  end
end

puts Child.build("a", "b")
puts Child.wrap("hi")
puts GrandChild.build("x", "y")

# super in inherited hook forwards the subclass arg
$log = []
class Watcher
  def self.inherited(sub)
    $log << "W:#{sub.name}"
  end
end
class Observer < Watcher
  def self.inherited(sub)
    $log << "O:#{sub.name}"
    super
  end
end
class Spy < Observer; end
puts $log.sort.join(",")
