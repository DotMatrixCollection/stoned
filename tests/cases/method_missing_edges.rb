# method_missing edge cases

# super from method_missing falls through to NoMethodError
class Strict
  def method_missing(name, *args)
    return "got:#{name}" if name == :known
    super
  end

  def respond_to_missing?(name, include_private = false)
    name == :known
  end
end

s = Strict.new
puts s.known          # got:known
begin
  s.unknown
rescue NoMethodError => e
  puts e.class        # NoMethodError
end

# method_missing inherited through a chain
class Base
  def method_missing(name, *args)
    "base:#{name}"
  end
end

class Child < Base
end

puts Child.new.anything   # base:anything

# method_missing on a module-mixed class
module Ghost
  def method_missing(name, *args)
    name.to_s.start_with?("ghost_") ? "haunted" : super
  end

  def respond_to_missing?(name, include_private = false)
    name.to_s.start_with?("ghost_") || super
  end
end

class House
  include Ghost
end

h = House.new
puts h.ghost_room          # haunted
puts h.respond_to?(:ghost_room)   # true
begin
  h.normal_room
rescue NoMethodError => e
  puts e.class   # NoMethodError
end

# respond_to_missing? default is false when not defined
class Plain
end

puts Plain.new.respond_to?(:nope)  # false

# method_missing receives symbol name
class Recorder
  attr_reader :last_name
  def method_missing(name, *args)
    @last_name = name
  end
end

r = Recorder.new
r.hello
puts r.last_name.class   # Symbol
puts r.last_name         # hello
