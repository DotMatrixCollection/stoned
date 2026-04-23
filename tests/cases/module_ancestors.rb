# Module ancestor ordering and super dispatch

module M1
  def who; "M1"; end
  def chain; "M1>#{super}"; end
end

module M2
  def who; "M2"; end
  def chain; "M2>#{super}"; end
end

module M3
  def chain; "M3>#{super}"; end
end

class Base
  def who; "Base"; end
  def chain; "Base"; end
end

# Single include: [Child, M1, Base]
class Child < Base
  include M1
end

puts Child.ancestors.first(3).map(&:name).inspect  # ["Child", "M1", "Base"]
puts Child.new.who    # M1  (module wins over Base)
puts Child.new.chain  # M1>Base

# Multiple includes — last include is closest to class
class Multi < Base
  include M1
  include M2
end

puts Multi.ancestors.first(4).map(&:name).inspect  # ["Multi", "M2", "M1", "Base"]
puts Multi.new.who    # M2
puts Multi.new.chain  # M2>M1>Base

# Module including a module
module Outer
  include M3
  def chain; "Outer>#{super}"; end
end

class Layered < Base
  include Outer
end

puts Layered.ancestors.first(4).map(&:name).inspect  # ["Layered", "Outer", "M3", "Base"]
puts Layered.new.chain  # Outer>M3>Base

# prepend — module comes BEFORE the class
module Prepended
  def who; "Pre>#{super}"; end
end

class WithPrepend < Base
  prepend Prepended
  def who; "WithPrepend"; end
end

puts WithPrepend.ancestors.first(3).map(&:name).inspect  # ["Prepended", "WithPrepend", "Base"]
puts WithPrepend.new.who  # Pre>WithPrepend

# super in class method
module ClassMod
  def build; "ClassMod:#{super}"; end
end

class Factory
  extend ClassMod
  def self.build; "Factory"; end
end

puts Factory.build  # ClassMod:Factory

# super with explicit args
class Adder
  def add(a, b); a + b; end
end

class LoggedAdder < Adder
  def add(a, b)
    result = super(a, b)
    result
  end
end

puts LoggedAdder.new.add(3, 4)  # 7

# super with no args forwards all params
class Doubler < Adder
  def add(a, b)
    super  # forwards a, b implicitly
  end
end

puts Doubler.new.add(5, 6)  # 11
