class Person
  attr_reader :name
  private
  attr_writer :name
  protected
  attr_reader :secret
  public
  def initialize(n, s)
    @name = n
    @secret = s
  end
  def rename(n) = (@name = n)
  def compare_secret(other) = secret == other.secret
end

p = Person.new("Bob", "xyz")
puts p.name

begin
  p.name = "Alice"
rescue NoMethodError => e
  puts e.message
end

p.rename("Charlie")
puts p.name

begin
  p.secret
rescue NoMethodError => e
  puts e.message
end

p2 = Person.new("Dave", "xyz")
puts p.compare_secret(p2)

class Account
  attr_reader :balance
  private
  attr_accessor :pin
  public
  def initialize(b, p_in)
    @balance = b
    @pin = p_in
  end
end

a = Account.new(100, 1234)
puts a.balance

begin
  a.pin
rescue NoMethodError => e
  puts "private: #{e.message[0, 7]}"
end

begin
  a.pin = 9999
rescue NoMethodError => e
  puts "private: #{e.message[0, 7]}"
end
