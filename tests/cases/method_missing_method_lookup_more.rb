class DynResponder
  def respond_to_missing?(name, include_private = false)
    name == :dynamic || super
  end

  def method_missing(name, *args)
    return :ok if name == :dynamic
    super
  end
end

d = DynResponder.new
p d.respond_to?(:dynamic)
p d.method(:dynamic).call
