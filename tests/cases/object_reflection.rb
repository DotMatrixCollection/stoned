# Object-level reflection: methods, public_methods, private_methods, protected_methods, method

class Widget
  def visible; end
  def also_visible; end

  protected
  def guarded; end

  private
  def hidden; end
end

w = Widget.new

# methods returns public + protected
puts w.methods.include?(:visible)      # true
puts w.methods.include?(:guarded)      # true
puts w.methods.include?(:hidden)       # false
puts w.methods.include?(:class)        # true (inherited from Object)

# public_methods
puts w.public_methods.include?(:visible)  # true
puts w.public_methods.include?(:guarded)  # false
puts w.public_methods.include?(:hidden)   # false

# private_methods
puts w.private_methods.include?(:hidden)   # true
puts w.private_methods.include?(:visible)  # false

# protected_methods
puts w.protected_methods.include?(:guarded)   # true
puts w.protected_methods.include?(:visible)   # false

# methods(false) — own only (singleton + own class)
puts w.public_methods(false).include?(:visible)  # true
puts w.public_methods(false).include?(:class)    # false (inherited)

# Object#method — returns a callable Method
m = w.method(:visible)
puts m.class          # Method
puts m.arity          # 0
puts m.call.inspect   # nil (no return value, but doesn't blow up)

m2 = 42.method(:+)
puts m2.call(8)       # 50

# Method#to_proc
plus = 1.method(:+)
puts [2, 3, 4].map(&plus).inspect  # [3, 4, 5]

# calling method on a non-existent method raises NameError
begin
  w.method(:nope)
rescue NameError => e
  puts e.class   # NameError
end

# calling method on a private method returns it (send semantics)
priv = w.method(:hidden)
puts priv.call.inspect  # nil

# unbound method
um = Widget.instance_method(:visible)
puts um.class  # UnboundMethod
bound = um.bind(w)
puts bound.class  # Method
puts bound.call.inspect  # nil
