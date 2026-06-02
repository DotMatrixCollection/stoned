# Method objects: unbind, bind, owner, receiver
class Greeter
  def hello(name) = "Hello, #{name}!"
  def bye(name)   = "Bye, #{name}!"
end

g = Greeter.new
m = g.method(:hello)
p m.call("World")
p m.arity
p m.owner
p m.receiver.equal?(g)
p m.name

# UnboundMethod
um = Greeter.instance_method(:hello)
p um.class
p um.owner
p um.arity
p um.name

# bind to another instance of same class
g2 = Greeter.new
bound = um.bind(g2)
p bound.call("Ruby")

# Method as block via &
p ["Alice", "Bob"].map(&g.method(:hello))

# to_proc
pr = m.to_proc
p pr.call("Proc")
p pr.lambda?

# method(:puts) works on kernel methods
p_method = method(:puts)
p p_method.class
p_method.call("via method object")

# Comparable arity
p method(:puts).arity
p method(:p).arity
