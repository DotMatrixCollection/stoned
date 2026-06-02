# respond_to? checks method availability
p "hello".respond_to?(:upcase)
p "hello".respond_to?(:nonexistent)
p 42.respond_to?(:times)
p 42.respond_to?(:each)
p [].respond_to?(:push)
p {}.respond_to?(:merge)

# Second arg true includes private methods
class Secrets
  def public_info = "public"
  private
  def hidden = "secret"
end
s = Secrets.new
p s.respond_to?(:public_info)
p s.respond_to?(:hidden)
p s.respond_to?(:hidden, true)

# respond_to_missing? integrates with respond_to?
class DynamicProxy
  def initialize(target)
    @target = target
  end

  def method_missing(name, *args, &block)
    if @target.respond_to?(name)
      @target.send(name, *args, &block)
    else
      super
    end
  end

  def respond_to_missing?(name, include_private = false)
    @target.respond_to?(name, include_private) || super
  end
end

proxy = DynamicProxy.new("hello")
p proxy.respond_to?(:upcase)
p proxy.respond_to?(:length)
p proxy.respond_to?(:nonexistent)
p proxy.upcase
p proxy.length

# respond_to? on nil
p nil.respond_to?(:to_s)
p nil.respond_to?(:upcase)
