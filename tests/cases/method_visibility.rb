class SecretBox
  def open
    "open"
  end

  def reveal
    secret
  end

  private

  def secret
    "secret"
  end
end

box = SecretBox.new
puts box.send(:secret)
puts box.reveal
puts box.respond_to?(:secret)

begin
  box.secret
rescue NoMethodError => e
  puts e.class
end

begin
  box.public_send(:secret)
rescue NoMethodError => e
  puts e.class
end

class SwitchBox
  def visible
    "visible"
  end

  def hidden
    "hidden"
  end

  private :hidden
end

sw = SwitchBox.new
puts sw.respond_to?(:visible)
puts sw.respond_to?(:hidden)
puts sw.send(:hidden)

class Parent
  protected

  def token
    "token"
  end
end

class Child < Parent
  def same_family(other)
    other.token
  end
end

puts Child.new.same_family(Child.new)

begin
  Child.new.token
rescue NoMethodError => e
  puts e.class
  puts e.message
end

begin
  Child.new.public_send(:token)
rescue NoMethodError => e
  puts e.class
  puts e.message
end
