class Vault
  def self.open
    "open"
  end

  def self.hidden
    "hidden"
  end

  private_class_method :hidden
end

puts Vault.open
puts Vault.respond_to?(:open)
puts Vault.respond_to?(:hidden)
puts Vault.send(:hidden)

begin
  Vault.hidden
rescue NoMethodError => e
  puts e.class
end

begin
  Vault.public_send(:hidden)
rescue NoMethodError => e
  puts e.class
end

class Switchboard
  def self.a
    "a"
  end

  def self.b
    "b"
  end

  private_class_method :a, :b
  public_class_method :b
end

puts Switchboard.respond_to?(:a)
puts Switchboard.respond_to?(:b)
puts Switchboard.send(:a)
puts Switchboard.b
