class Proxy
  def initialize(target)
    @target = target
  end

  def method_missing(name, *args, &block)
    @target.send(name, *args, &block)
  end

  def respond_to_missing?(name, include_private = false)
    @target.respond_to?(name, include_private) || super
  end
end

p = Proxy.new([1, 2, 3])
puts p.respond_to?(:push)
puts p.method(:push).class
puts p.method(:length).call
p.method(:push).call(4)
puts p.method(:length).call
