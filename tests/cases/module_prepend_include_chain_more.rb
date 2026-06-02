module IncludedGreeting
  def greet
    super + ":included"
  end
end

module PrependedGreeting
  def greet
    "prepended:" + super
  end
end

class GreetingBase
  def greet
    "base"
  end
end

class GreetingChild < GreetingBase
  include IncludedGreeting
  prepend PrependedGreeting
end

obj = GreetingChild.new
ancestors = GreetingChild.ancestors
p obj.greet
p ancestors.index(PrependedGreeting) < ancestors.index(GreetingChild)
p ancestors.index(IncludedGreeting) > ancestors.index(GreetingChild)
p ancestors.index(IncludedGreeting) < ancestors.index(GreetingBase)
