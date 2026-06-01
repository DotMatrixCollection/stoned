class BankAccount
  def initialize(balance)
    @balance = balance
  end

  def >(other)
    balance > other.balance
  end

  def deposit(amount)
    @balance += amount
  end

  protected

  def balance
    @balance
  end
end

a1 = BankAccount.new(100)
a2 = BankAccount.new(200)
p a1 > a2
p a2 > a1

begin
  a1.balance
rescue NoMethodError => e
  p e.message.include?("protected")
end
