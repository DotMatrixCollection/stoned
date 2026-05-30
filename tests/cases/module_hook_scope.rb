# Nested module hooks must not inherit from outer module
events = []

module Outer
  def self.extended(base)
    events << "Outer.extended(#{base})"
  end

  def self.included(base)
    events << "Outer.included(#{base})"
  end

  module Inner
    def self.extended(base)
      events << "Inner.extended(#{base})"
    end
  end
end

class A
  include Outer
end

class B
  extend Outer::Inner
end

# Only "Outer.included(A)" and "Inner.extended(B)" should fire
puts events.sort.join(", ")

# const_set on Object registers globally accessible constants
Object.const_set("DynamicClass", Class.new { def hello; "hi"; end })
puts DynamicClass.new.hello
puts Object.const_defined?("DynamicClass")
