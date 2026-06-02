class BaseGreeter
  def greet(name)
    "hello #{name}"
  end
end

class LoudGreeter < BaseGreeter
  def greet(name)
    super.upcase
  end
end

class ExcitedGreeter < LoudGreeter
  def greet(name)
    super + "!"
  end
end

g = ExcitedGreeter.new
p g.greet("ruby")
p g.is_a?(BaseGreeter)
p g.class.superclass
p g.class.superclass.superclass
