class Vault
  def open; secret; end
  private
  def secret; "treasure"; end
end
v = Vault.new
puts v.open
begin
  v.secret
rescue NoMethodError => e
  puts e.message.start_with?("private method")
end
puts v.send(:secret)
