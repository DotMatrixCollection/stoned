# freeze, frozen?, dup, clone, FrozenError

# objects start unfrozen
class Box
  attr_accessor :val
  def initialize(v); @val = v; end
end

b = Box.new(1)
puts b.frozen?   # false
b.freeze
puts b.frozen?   # true

# frozen object raises FrozenError when a setter is called
begin
  b.val = 2
rescue => e
  puts e.class   # FrozenError
end

# freeze returns self
x = Box.new(0)
puts x.freeze.equal?(x)  # true

# integers and symbols are always frozen
puts 42.frozen?    # true
puts :sym.frozen?  # true
puts true.frozen?  # true
puts nil.frozen?   # true

# dup produces an unfrozen copy with copied ivars
orig = Box.new(42)
orig.freeze
copy = orig.dup
puts copy.frozen?      # false
puts copy.val          # 42
puts copy.equal?(orig) # false

# dup copy is independent
copy.val = 99
puts orig.val  # 42  (original unchanged)

# clone preserves frozen state
frozen_box = Box.new(7).freeze
c = frozen_box.clone
puts c.frozen?  # true
puts c.val      # 7

# FrozenError is a subclass of RuntimeError
puts FrozenError.ancestors.include?(RuntimeError)  # true
