$stdout.sync = true

# class_eval with block: defs go to the class as instance methods
class Widget
end

Widget.class_eval do
  def name
    "widget"
  end

  def active?
    true
  end
end

puts Widget.new.name
puts Widget.new.active?

# module_eval — same as class_eval
module Greeter
end

Greeter.module_eval do
  def greet(who)
    "Hello, #{who}!"
  end
end

class App
  include Greeter
end

puts App.new.greet("World")

# class_eval with string
class Config
end

Config.class_eval("def version; '2.0'; end")
puts Config.new.version

# instance_exec: block gets args, self bound to receiver
class Builder
  def initialize
    @parts = []
  end

  def add(part)
    @parts << part
  end

  def parts
    @parts
  end
end

b = Builder.new
b.instance_exec("alpha", "beta") do |a, c|
  add a
  add c
  add "gamma"
end
puts b.parts.join(",")

# def inside block inside class body hoists to the class
class Counter
  [1, 2].each do |n|
    define_method("step_#{n}") { n }
  end

  3.times do
    def heartbeat
      "beat"
    end
  end
end

c = Counter.new
puts c.step_1
puts c.step_2
puts c.heartbeat
