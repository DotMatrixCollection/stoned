# User-defined methods must take priority over built-in Kernel handlers
class Request
  def method
    "GET"
  end

  def class
    "my-class-string"
  end

  def respond_to?(name, _ = false)
    name == :method || name == :class
  end
end

r = Request.new
puts r.method
puts r.class
puts r.respond_to?(:method)
puts r.respond_to?(:nonexistent)
