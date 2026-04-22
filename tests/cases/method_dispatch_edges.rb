class ArityDemo
  def zero
    :ok
  end

  def one(x)
    x
  end
end

begin
  p ArityDemo.new.zero(1)
rescue => e
  puts e.class
end

begin
  p ArityDemo.new.one
rescue => e
  puts e.class
end

class PublicSendDemo
  private

  def secret
    :ok
  end

  def method_missing(name, *args)
    return :fallback if name == :secret
    super
  end

  def respond_to_missing?(name, include_private)
    name == :secret
  end
end

begin
  p PublicSendDemo.new.public_send(:secret)
rescue => e
  puts e.class
end
