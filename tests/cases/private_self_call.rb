class Foo
  private
  def secret; "shh"; end
  attr_writer :val
  public
  attr_reader :val
  def initialize(v); @val = v; end
  def reveal; self.secret; end
  def set(v); self.val = v; end
end

f = Foo.new("x")
puts f.reveal
puts f.val
f.set("y")
puts f.val

begin
  f.secret
rescue NoMethodError => e
  puts e.message
end

begin
  f.val = "z"
rescue NoMethodError => e
  puts e.message
end
