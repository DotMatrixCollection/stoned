class DynamicThing
  def method_missing(name, *args)
    if name == :dyn_hello
      "dyn:" + args.join("-")
    else
      "other"
    end
  end

  def respond_to_missing?(name, include_private)
    name == :dyn_hello
  end
end

thing = DynamicThing.new
puts thing.dyn_hello(1, 2)
puts thing.respond_to?(:dyn_hello)
puts thing.respond_to?(:nope)

class DynamicClass
  def self.method_missing(name, *args)
    if name == :build
      "build:" + args.join("-")
    else
      "other"
    end
  end

  def self.respond_to_missing?(name, include_private)
    name == :build
  end
end

puts DynamicClass.build("x")
puts DynamicClass.respond_to?(:build)
puts DynamicClass.respond_to?(:other)
