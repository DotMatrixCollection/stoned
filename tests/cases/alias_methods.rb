class Greeter
  def hello
    "hello"
  end

  alias hi hello
  alias_method :wave, :hello

  def +(n)
    n + 1
  end

  alias plus +
  alias_method :sum, :+

  def hello
    "new hello"
  end
end

g = Greeter.new
puts g.hi
puts g.wave
puts g.hello
puts g.plus(4)
puts g.sum(5)

class ParentAlias
  protected

  def secret
    "secret"
  end
end

class ChildAlias < ParentAlias
  alias_method :copy_secret, :secret

  def call_copy(other)
    other.copy_secret
  end
end

c = ChildAlias.new
puts c.call_copy(c)

begin
  class BrokenAlias
    alias nope missing
  end
rescue NameError => e
  puts e.class
end

begin
  class BrokenAliasMethod
    alias_method :nope, :missing
  end
rescue NameError => e
  puts e.class
end
