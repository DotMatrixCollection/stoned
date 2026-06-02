# dup and clone for copying objects
class Config
  attr_accessor :name, :values
  def initialize(n, v)
    @name, @values = n, v.dup
  end
end

c1 = Config.new("original", [1, 2, 3])
c2 = c1.dup
c2.name = "copy"
c2.values << 4

p c1.name
p c2.name
p c1.values
p c2.values

# frozen objects cannot be duped-to-mutate
str = "hello".freeze
p str.frozen?

dup_str = str.dup
p dup_str.frozen?  # dup removes frozen status
dup_str << " world"
p dup_str

clone_str = str.clone
p clone_str.frozen?  # clone preserves frozen status

begin
  clone_str << " world"
rescue => e
  p e.class
end

# freeze returns self
arr = [1, 2, 3]
p arr.freeze.equal?(arr)
p arr.frozen?

begin
  arr << 4
rescue => e
  p e.class
end
p arr
