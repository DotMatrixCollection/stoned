# Protected methods are callable from within the same class hierarchy
class Account
  def initialize(balance)
    @balance = balance
  end

  def >(other)
    balance > other.balance
  end

  def ==(other)
    balance == other.balance
  end

  def to_s
    "Account(#{balance})"
  end

  protected

  def balance
    @balance
  end
end

a1 = Account.new(100)
a2 = Account.new(200)
a3 = Account.new(100)

p a1 > a2
p a2 > a1
p a1 == a3

# Protected not callable from outside
begin
  a1.balance
rescue NoMethodError => e
  p e.message.include?("protected")
end

# Subclass can call protected methods of parent
class SavingsAccount < Account
  def initialize(balance, rate)
    super(balance)
    @rate = rate
  end

  def interest
    balance * @rate
  end
end

s = SavingsAccount.new(1000, 0.05)
p s.interest

# protected vs private: protected allows same-class cross-instance calls
class Box
  def initialize(v) = @v = v
  def same?(other)  = secret == other.secret
  protected
  def secret = @v
end

p Box.new(42).same?(Box.new(42))
p Box.new(42).same?(Box.new(99))
