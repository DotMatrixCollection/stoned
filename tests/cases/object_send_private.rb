# send bypasses visibility, public_send does not
class Safe
  def pub = "public"
  private
  def sec = "secret"
  protected
  def prot = "protected"
end

s = Safe.new
p s.pub
p s.send(:sec)
p s.send(:prot)

begin
  s.public_send(:sec)
rescue NoMethodError => e
  p e.message.include?("private")
end

# send with arguments
class Calc
  private
  def multiply(a, b) = a * b
end
c = Calc.new
p c.send(:multiply, 6, 7)

# __send__ is an alias
p s.__send__(:pub)
p s.__send__(:sec)

# send works with symbols and strings
p s.send("pub")

# Dynamic dispatch with send
methods = [:upcase, :downcase, :reverse]
p methods.map { |m| "Hello".send(m) }
