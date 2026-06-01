class Base
  def greet; "hello from Base"; end
end

class Child < Base
  def greet; "hello from Child"; end
  remove_method :greet
end
puts Child.new.greet

class Child2 < Base
  def greet; "hello from Child2"; end
  undef_method :greet
end
begin
  Child2.new.greet
rescue NoMethodError => e
  puts e.message
end

class Watcher
  EVENTS = []
  def self.method_added(m);    EVENTS << "added:#{m}";    end
  def self.method_removed(m);  EVENTS << "removed:#{m}";  end
  def self.method_undefined(m); EVENTS << "undefined:#{m}"; end
  def alpha; end
  def beta; end
  remove_method :alpha
  undef_method :beta
end
puts Watcher::EVENTS.inspect
