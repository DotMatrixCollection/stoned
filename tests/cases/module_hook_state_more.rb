module HookState
  @@events = []

  def self.events
    @@events
  end

  def self.included(base)
    @@events << "included:#{base}"
  end

  def self.extended(obj)
    @@events << "extended:#{obj.class}"
  end
end

class HookStateHost
  include HookState
end

obj = Object.new
obj.extend(HookState)

p HookState.events
