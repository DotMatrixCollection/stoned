# method_added fires when the hook is defined via extend (not just direct def self.)
module Hooks
  def method_added(name)
    (@added_methods ||= []) << name.to_s
  end
  def added_methods; @added_methods || []; end
end

class Widget
  extend Hooks
  def draw; end
  def resize; end
end

puts Widget.added_methods.sort.inspect
